#include "UpdatePanel.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include <QtTest>

class UpdateTests : public QObject {
    Q_OBJECT
private slots:
    void privateDiscoveryAndVerifiedDownload()
    {
        QSettings().setValue(QStringLiteral("updates/checkAtStartup"), false);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        const QString assetPath = QStringLiteral("/repos/victorhmrod/MediaSuite/releases/assets/1");
        const QUrl api(base + QStringLiteral("/repos/victorhmrod/MediaSuite/releases/latest"));
        const QByteArray installer("MZ-test-installer-data");
        const QString target = QStringLiteral("MediaSuiteUpdateTests/") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject release{{QStringLiteral("tag_name"), QStringLiteral("v99.0.0")}};
        bool corrupt = false;
        bool badRedirect = false;
        bool unauthorized = false;
        int requests = 0;
        bool authorized = true;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n")) return;
                ++requests;
                authorized = authorized && request.toLower().contains("authorization: bearer test-only-token");
                const QByteArray path = request.split(' ').value(1);
                if (unauthorized) {
                    socket->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                if (path == assetPath.toUtf8()) {
                    const QByteArray location = badRedirect ? QByteArray("https://example.invalid/stolen-token") : QByteArray("/installer.exe");
                    socket->write("HTTP/1.1 302 Found\r\nLocation: " + location + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                QByteArray body = path == "/installer.exe" ? installer : QJsonDocument(release).toJson(QJsonDocument::Compact);
                if (path == "/installer.exe" && corrupt) body[body.size() - 1] = 'X';
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });

        UpdatePanel panel(nullptr, api, target);
        panel.show();
        auto* status = panel.findChild<QLabel*>(QStringLiteral("updateStatus"));
        auto* token = panel.findChild<QLineEdit*>(QStringLiteral("updateToken"));
        auto* save = panel.findChild<QPushButton*>(QStringLiteral("saveUpdateToken"));
        auto* check = panel.findChild<QPushButton*>(QStringLiteral("checkForUpdates"));
        auto* download = panel.findChild<QPushButton*>(QStringLiteral("downloadUpdate"));
        auto* install = panel.findChild<QPushButton*>(QStringLiteral("installUpdate"));
        QSignalSpy available(&panel, &UpdatePanel::updateAvailable);
        QSignalSpy installRequested(&panel, &UpdatePanel::installRequested);
        check->click();
        QVERIFY(status->text().contains(QStringLiteral("Save a GitHub token")));
        QCOMPARE(requests, 0);
        token->setText(QStringLiteral("test-only-token"));
        save->click();
        QVERIFY(token->text().isEmpty());
        QVERIFY(status->text().contains(QStringLiteral("saved securely")));
        {
            UpdatePanel persisted(nullptr, api, target);
            QVERIFY(persisted.findChild<QLineEdit*>(QStringLiteral("updateToken"))->text().isEmpty());
            QVERIFY(persisted.findChild<QLineEdit*>(QStringLiteral("updateToken"))->placeholderText().contains(QStringLiteral("saved")));
        }
        release[QStringLiteral("tag_name")] = QStringLiteral("v") + QCoreApplication::applicationVersion();
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("up to date")));
        QVERIFY(!download->isVisible());

        release[QStringLiteral("tag_name")] = QStringLiteral("v99.0.0");
        QJsonObject asset{{QStringLiteral("name"), QStringLiteral("MediaSuite-99.0.0-setup.exe")},
            {QStringLiteral("state"), QStringLiteral("uploaded")}, {QStringLiteral("url"), base + assetPath},
            {QStringLiteral("size"), installer.size()}, {QStringLiteral("digest"), QStringLiteral("sha256:") + QString::fromLatin1(QCryptographicHash::hash(installer, QCryptographicHash::Sha256).toHex())}};
        release[QStringLiteral("assets")] = QJsonArray{asset};
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QCOMPARE(available.count(), 1);
        QVERIFY(download->isVisible());
        download->click();
        QTRY_VERIFY_WITH_TIMEOUT(install->isVisible(), 5000);
        QVERIFY(status->text().contains(QStringLiteral("SHA-256 verified")));
        QVERIFY(authorized);
        install->click();
        QCOMPARE(installRequested.count(), 1);
        const QString fileName = installRequested.first().first().toString();
        QFile downloaded(fileName);
        QVERIFY(downloaded.open(QIODevice::ReadOnly));
        QCOMPARE(downloaded.readAll(), installer);
        downloaded.close();
        // Installation rechecks integrity after download, rather than trusting the file path.
        QVERIFY(downloaded.open(QIODevice::WriteOnly));
        downloaded.write("tampered");
        downloaded.close();
        install->click();
        QCOMPARE(installRequested.count(), 1);
        QVERIFY(status->text().contains(QStringLiteral("verification failed")));

        corrupt = true;
        check->click();
        QTRY_VERIFY(check->isEnabled());
        download->click();
        QTRY_VERIFY(download->isEnabled());
        QVERIFY(!install->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("failed verification")));
        corrupt = false;
        badRedirect = true;
        check->click();
        QTRY_VERIFY(check->isEnabled());
        download->click();
        QTRY_VERIFY(download->isEnabled());
        QVERIFY(!install->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("failed verification")));
        badRedirect = false;

        asset[QStringLiteral("url")] = QStringLiteral("https://example.invalid/installer.exe");
        release[QStringLiteral("assets")] = QJsonArray{asset};
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QVERIFY(!download->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("no verified Windows installer")));
        asset[QStringLiteral("url")] = base + assetPath;
        release[QStringLiteral("assets")] = QJsonArray{asset};
        const auto digest = asset.value(QStringLiteral("digest"));
        asset.remove(QStringLiteral("digest"));
        release[QStringLiteral("assets")] = QJsonArray{asset};
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QVERIFY(!download->isVisible());
        asset[QStringLiteral("digest")] = digest;
        release[QStringLiteral("assets")] = QJsonArray{asset};
        release[QStringLiteral("prerelease")] = true;
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QVERIFY(!download->isVisible());
        release[QStringLiteral("prerelease")] = false;
        unauthorized = true;
        check->click();
        QTRY_VERIFY(check->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("HTTP 401")));
        unauthorized = false;
        QSettings().setValue(QStringLiteral("updates/checkAtStartup"), true);
        {
            UpdatePanel automatic(nullptr, api, target);
            QSignalSpy startup(&automatic, &UpdatePanel::updateAvailable);
            QTRY_COMPARE_WITH_TIMEOUT(startup.count(), 1, 7000);
        }
        QSettings().setValue(QStringLiteral("updates/checkAtStartup"), false);
        token->clear();
        save->click();
        check->click();
        QVERIFY(status->text().contains(QStringLiteral("Save a GitHub token")));
        QFile::remove(fileName);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MediaSuiteUpdateTests"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MEDIA_SUITE_VERSION));
    QStandardPaths::setTestModeEnabled(true);
    UpdateTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_updates.moc"
