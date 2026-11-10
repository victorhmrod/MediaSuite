#include "ApplicationController.h"
#include "DownloadsView.h"
#include "MainWindow.h"
#include "MpvBackend.h"
#include "Theme.h"
#include "ui/MainWindow.h"
#include "core/ConversionJob.h"
#include "core/JobManager.h"
#include "UpdatePanel.h"

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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest>

class SuiteTest : public QObject {
    Q_OBJECT
private slots:
    void downloadImportAndNavigation()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString settingsOrganization = QStringLiteral("MediaSuiteTests-") + QFileInfo(temp.path()).fileName();
        QSettings().setValue(QStringLiteral("updates/checkAtStartup"), false);
        qputenv("MEDIA_SUITE_SETTINGS_ORGANIZATION", settingsOrganization.toUtf8());
        const QString clip = temp.filePath(QStringLiteral("original.mp4"));
        QProcess ffmpeg;
        ffmpeg.start(QStringLiteral("ffmpeg"), {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-f"), QStringLiteral("lavfi"),
            QStringLiteral("-i"), QStringLiteral("color=c=blue:s=160x90:r=25"), QStringLiteral("-f"), QStringLiteral("lavfi"),
            QStringLiteral("-i"), QStringLiteral("sine=frequency=440"), QStringLiteral("-t"), QStringLiteral("0.5"),
            QStringLiteral("-c:a"), QStringLiteral("aac"), clip});
        QVERIFY(ffmpeg.waitForFinished(15000));
        QCOMPARE(ffmpeg.exitCode(), 0);
        QFile file(clip);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray video = file.readAll();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, video, &server] {
                const QByteArray request = socket->readAll();
                QByteArray content = video;
                QByteArray contentType("video/mp4");
                if (request.startsWith("GET /playlist.xspf")) {
                    content = QStringLiteral("<?xml version=\"1.0\"?><playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\"><title>Download playlist</title><trackList><track><location>http://127.0.0.1:%1/first.mp4</location><title>First</title></track><track><location>http://127.0.0.1:%1/second.mp4</location><title>Second</title></track></trackList></playlist>").arg(server.serverPort()).toUtf8();
                    contentType = "application/xspf+xml";
                }
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\nContent-Length: " + QByteArray::number(content.size()) + "\r\nConnection: close\r\n\r\n");
                if (!request.startsWith("HEAD")) socket->write(content);
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
        for (auto* combo : tools->findChildren<QComboBox*>()) {
            const int cpu = combo->findData(QStringLiteral("cpu"));
            if (cpu >= 0) combo->setCurrentIndex(cpu);
        }
        auto* output = downloads->findChild<QComboBox*>(QStringLiteral("downloadOutputFormat"));
        QVERIFY(output->findData(QStringLiteral("mp3")) >= 0);
        QVERIFY(output->findData(QStringLiteral("webm")) >= 0);
        QVERIFY(output->findData(QStringLiteral("png")) >= 0);
        QVERIFY(output->findData(QStringLiteral("pdf")) < 0);
        output->setCurrentIndex(output->findData(QStringLiteral("mkv")));
        startDownload->click();
        auto* card = downloads->findChild<DownloadCard*>();
        QVERIFY(card);
        QSignalSpy imported(card, &DownloadCard::importRequested);
        QTRY_COMPARE_WITH_TIMEOUT(imported.count(), 1, 45000);
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 10000);
        const QString downloaded = imported.first().first().toString();
        QVERIFY(downloaded.endsWith(QStringLiteral(".mkv")));
        QVERIFY(QFile::exists(downloaded));
        QVERIFY(downloads->isVisible());
        const QString original = QFileInfo(downloaded).dir().filePath(QStringLiteral("fixture.mp4"));
        QVERIFY(QFile::exists(original));
        QTRY_COMPARE_WITH_TIMEOUT(controller.library().movies().size(), 1, 10000);
        auto* conversionFormat = card->findChild<QComboBox*>(QStringLiteral("conversionFormat"));
        auto* convert = card->findChild<QPushButton*>(QStringLiteral("convertDownload"));
        for (const auto& extension : {QStringLiteral("mp3"), QStringLiteral("png"), QStringLiteral("webm"), QStringLiteral("avi"), QStringLiteral("wmv"), QStringLiteral("flv")}) {
            conversionFormat->setCurrentIndex(conversionFormat->findData(extension));
            convert->click();
            QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 30000);
            auto* job = tools->jobManager().jobs().last();
            QVERIFY2(job->status() == magnify::core::JobStatus::Completed, qPrintable(job->errorMessage()));
            QVERIFY(QFile::exists(job->outputPath()));
            QVERIFY(QFile::exists(original));
            QVERIFY(downloads->isVisible());
        }
        // Failure and retry reuse the downloaded source instead of downloading again.
        const QString backup = original + QStringLiteral(".backup");
        QVERIFY(QFile::rename(original, backup));
        conversionFormat->setCurrentIndex(conversionFormat->findData(QStringLiteral("mp3")));
        convert->click();
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 10000);
        QCOMPARE(tools->jobManager().jobs().last()->status(), magnify::core::JobStatus::Failed);
        QVERIFY(QFile::rename(backup, original));
        card->findChild<QPushButton*>(QStringLiteral("retryDownload"))->click();
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 10000);
        QCOMPARE(tools->jobManager().jobs().last()->status(), magnify::core::JobStatus::Completed);
        // Preserve an existing result when converting to that format again.
        QFile sentinel(QFileInfo(original).dir().filePath(QStringLiteral("fixture.mov")));
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        sentinel.write("keep this file");
        sentinel.close();
        conversionFormat->setCurrentIndex(conversionFormat->findData(QStringLiteral("mov")));
        convert->click();
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 10000);
        QCOMPARE(tools->jobManager().jobs().last()->status(), magnify::core::JobStatus::Completed);
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), QByteArray("keep this file"));
        sentinel.close();
        conversionFormat->setCurrentIndex(conversionFormat->findData(QStringLiteral("webm")));
        convert->click();
        card->cancel();
        card->findChild<QPushButton*>(QStringLiteral("retryDownload"))->click();
        QTRY_VERIFY_WITH_TIMEOUT(!card->active(), 30000);
        QVERIFY2(tools->jobManager().jobs().last()->status() == magnify::core::JobStatus::Completed,
            qPrintable(tools->jobManager().jobs().last()->errorMessage()));
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
        const int rows = table->rowCount();
        QTRY_VERIFY_WITH_TIMEOUT(table->rowCount() > rows, 10000);
        QTimer::singleShot(10000, &window, [] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                qWarning() << message->text();
                message->reject();
            }
        });
        for (auto* button : tools->findChildren<QPushButton*>())
            if (button->text() == QStringLiteral("Start queue")) button->click();
        QTRY_COMPARE_WITH_TIMEOUT(table->item(table->rowCount() - 1, 2)->text(), QStringLiteral("Completed"), 30000);
        const QString converted = tools->jobManager().jobs().last()->outputPath();
        QVERIFY(QFile::exists(converted));
        emit tools->importRequested(converted);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.importer().isBusy(), 10000);
        downloadNav->click();
        QVERIFY(downloads->isVisible());
        downloads->findChild<QCheckBox*>(QStringLiteral("autoImportDownload"))->setChecked(false);
        output->setCurrentIndex(output->findData(QStringLiteral("mp3")));
        url->setText(QStringLiteral("http://127.0.0.1:%1/playlist.xspf").arg(server.serverPort()));
        downloads->findChild<QPushButton*>(QStringLiteral("searchDownload"))->click();
        QTRY_VERIFY_WITH_TIMEOUT(startDownload->isEnabled(), 10000);
        const int beforeBatch = tools->jobManager().jobs().size();
        startDownload->click();
        DownloadCard* batch = nullptr;
        for (auto* item : downloads->findChildren<DownloadCard*>())
            if (item != card) batch = item;
        QVERIFY(batch);
        QTRY_VERIFY_WITH_TIMEOUT(!batch->active(), 30000);
        QCOMPARE(tools->jobManager().jobs().size(), beforeBatch + 2);
        for (int i = beforeBatch; i < tools->jobManager().jobs().size(); ++i) {
            auto* job = tools->jobManager().jobs()[i];
            QVERIFY2(job->status() == magnify::core::JobStatus::Completed, qPrintable(job->errorMessage()));
            QVERIFY(QFile::exists(job->inputPath()));
            QVERIFY(QFile::exists(job->outputPath()));
            QVERIFY(job->outputPath().endsWith(QStringLiteral(".mp3")));
        }
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
