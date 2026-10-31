#include "ApplicationController.h"
#include "DownloadsView.h"
#include "MainWindow.h"
#include "MpvBackend.h"
#include "Theme.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTableWidget>
#include <QTimer>
#include <QtTest>

class SuiteTest : public QObject {
    Q_OBJECT
private slots:
    void downloadImportAndNavigation()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString settingsOrganization = QStringLiteral("MediaSuiteTests-") + QFileInfo(temp.path()).fileName();
        qputenv("MEDIA_SUITE_SETTINGS_ORGANIZATION", settingsOrganization.toUtf8());
        const QString clip = temp.filePath(QStringLiteral("original.mp4"));
        QProcess ffmpeg;
        ffmpeg.start(QStringLiteral("ffmpeg"), {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-f"), QStringLiteral("lavfi"),
            QStringLiteral("-i"), QStringLiteral("color=c=blue:s=160x90:r=10"), QStringLiteral("-t"), QStringLiteral("0.5"), clip});
        QVERIFY(ffmpeg.waitForFinished(15000));
        QCOMPARE(ffmpeg.exitCode(), 0);
        QFile file(clip);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray video = file.readAll();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, video] {
                const QByteArray request = socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: " + QByteArray::number(video.size()) + "\r\nConnection: close\r\n\r\n");
                if (!request.startsWith("HEAD")) socket->write(video);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });

        lf::MpvBackend::Options options;
        options.audio = false;
        options.video = false;
        auto backend = std::make_unique<lf::MpvBackend>(options);
        QVERIFY(backend->isValid());
        auto* engine = backend.get();
        lf::ApplicationController controller(temp.filePath(QStringLiteral("library")), std::move(backend));
        QString error;
        QVERIFY2(controller.initialize(&error), qPrintable(error));
        lf::ui::MainWindow window(controller, engine);
        window.show();
        auto* downloads = window.findChild<DownloadsView*>();
        auto* tools = window.findChild<magnify::ui::MainWindow*>();
        auto* downloadNav = window.findChild<QToolButton*>(QStringLiteral("nav_downloads"));
        auto* toolsNav = window.findChild<QToolButton*>(QStringLiteral("nav_tools"));
        QVERIFY(downloads && tools && downloadNav && toolsNav);
        downloadNav->click();
        QVERIFY(downloads->isVisible());
        QVERIFY(downloadNav->isChecked());
        auto* url = downloads->findChild<QLineEdit*>(QStringLiteral("downloadUrl"));
        url->setText(QStringLiteral("https://invalid.example/video"));
        QVERIFY(!downloads->findChild<QPushButton*>(QStringLiteral("startDownload"))->isEnabled());

        url->setText(QStringLiteral("http://127.0.0.1:%1/fixture.mp4").arg(server.serverPort()));
        downloads->findChild<QPushButton*>(QStringLiteral("searchDownload"))->click();
        auto* startDownload = downloads->findChild<QPushButton*>(QStringLiteral("startDownload"));
        QTRY_VERIFY_WITH_TIMEOUT(startDownload->isEnabled(), 45000);
        downloads->findChild<QLineEdit*>(QStringLiteral("downloadDirectory"))->setText(temp.filePath(QStringLiteral("downloads with spaces \u2603")));
        downloads->findChild<QCheckBox*>(QStringLiteral("autoImportDownload"))->setChecked(true);
        startDownload->click();
        auto* card = downloads->findChild<DownloadCard*>();
        QVERIFY(card);
        QSignalSpy imported(card, &DownloadCard::importRequested);
        QTRY_COMPARE_WITH_TIMEOUT(imported.count(), 1, 45000);
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 10000);
        const QString downloaded = imported.first().first().toString();
        QVERIFY(QFile::exists(downloaded));
        QTRY_COMPARE_WITH_TIMEOUT(controller.library().movies().size(), 1, 10000);
        toolsNav->click();
        QVERIFY(tools->isVisible());
        QVERIFY(toolsNav->isChecked());
        QVERIFY(tools->styleSheet().isEmpty());
        QVERIFY(window.width() <= 1400);
        for (auto* combo : tools->findChildren<QComboBox*>()) {
            const int cpu = combo->findData(QStringLiteral("cpu"));
            if (cpu >= 0) combo->setCurrentIndex(cpu);
        }
        tools->openExternalFiles({downloaded}, QStringLiteral("mkv"));
        auto* table = tools->findChild<QTableWidget*>();
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 10000);
        QTimer::singleShot(10000, &window, [] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                qWarning() << message->text();
                message->reject();
            }
        });
        for (auto* button : tools->findChildren<QPushButton*>())
            if (button->text() == QStringLiteral("Start queue")) button->click();
        QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 2)->text(), QStringLiteral("Completed"), 30000);
        const QString converted = QFileInfo(downloaded).dir().filePath(QStringLiteral("fixture.mkv"));
        QVERIFY(QFile::exists(converted));
        emit tools->importRequested(converted);
        QTRY_COMPARE_WITH_TIMEOUT(controller.library().movies().size(), 2, 10000);
        downloadNav->click();
        QVERIFY(downloads->isVisible());
        window.close();
        QSettings(settingsOrganization, QStringLiteral("YTDownloader")).clear();
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MediaSuiteTests"));
    lf::ui::Theme::apply(app);
    SuiteTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_suite.moc"
