#include "UpdatePanel.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <windows.h>
#include <wincred.h>

UpdatePanel::UpdatePanel(QWidget* parent, QUrl releaseApi, QString credentialTarget)
    : QGroupBox(QStringLiteral("Application updates"), parent), m_apiUrl(std::move(releaseApi)), m_credentialTarget(std::move(credentialTarget))
{
    setObjectName(QStringLiteral("applicationUpdates"));
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(QStringLiteral("Private GitHub repository: use a fine-grained token with Contents: read access to MediaSuite. The token is stored in Windows Credential Manager."), this);
    description->setWordWrap(true);
    description->setProperty("muted", true);
    layout->addWidget(description);
    auto* credentials = new QHBoxLayout;
    m_token = new QLineEdit(this);
    m_token->setObjectName(QStringLiteral("updateToken"));
    m_token->setEchoMode(QLineEdit::Password);
    m_token->setPlaceholderText(token().isEmpty() ? QStringLiteral("GitHub token") : QStringLiteral("Token saved securely; leave blank and save to remove"));
    m_save = new QPushButton(QStringLiteral("Save token"), this);
    m_save->setObjectName(QStringLiteral("saveUpdateToken"));
    credentials->addWidget(m_token, 1);
    credentials->addWidget(m_save);
    layout->addLayout(credentials);
    m_automatic = new QCheckBox(QStringLiteral("Check for updates when Media Suite starts"), this);
    m_automatic->setChecked(m_settings.value(QStringLiteral("updates/checkAtStartup"), true).toBool());
    layout->addWidget(m_automatic);
    auto* actions = new QHBoxLayout;
    m_check = new QPushButton(QStringLiteral("Check for updates"), this);
    m_check->setObjectName(QStringLiteral("checkForUpdates"));
    m_download = new QPushButton(QStringLiteral("Download update"), this);
    m_download->setObjectName(QStringLiteral("downloadUpdate"));
    m_install = new QPushButton(QStringLiteral("Install update"), this);
    m_install->setObjectName(QStringLiteral("installUpdate"));
    m_download->setVisible(false);
    m_install->setVisible(false);
    for (auto* button : {m_check, m_download, m_install}) actions->addWidget(button);
    actions->addStretch();
    layout->addLayout(actions);
    m_status = new QLabel(QStringLiteral("Current version: %1").arg(QCoreApplication::applicationVersion()), this);
    m_status->setObjectName(QStringLiteral("updateStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    connect(m_save, &QPushButton::clicked, this, &UpdatePanel::OnSaveTokenCallback);
    connect(m_check, &QPushButton::clicked, this, &UpdatePanel::OnCheckCallback);
    connect(m_download, &QPushButton::clicked, this, &UpdatePanel::OnDownloadCallback);
    connect(m_install, &QPushButton::clicked, this, [this] {
        QFile file(m_readyInstaller);
        if (!file.open(QIODevice::ReadOnly)) {
            m_status->setText(QStringLiteral("The downloaded installer is missing. Download it again."));
            m_install->hide();
            return;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file) || hash.result().toHex() != m_expectedHash) {
            m_status->setText(QStringLiteral("Installer verification failed. Download the update again."));
            m_install->hide();
            return;
        }
        emit installRequested(m_readyInstaller);
    });
    connect(m_automatic, &QCheckBox::toggled, this, [this](bool enabled) { m_settings.setValue(QStringLiteral("updates/checkAtStartup"), enabled); });
    if (m_automatic->isChecked()) QTimer::singleShot(5000, this, [this] { if (m_automatic->isChecked() && !m_reply) OnCheckCallback(); });
}

UpdatePanel::~UpdatePanel()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

QByteArray UpdatePanel::token() const
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(m_credentialTarget.utf16()), CRED_TYPE_GENERIC, 0, &credential)) return {};
    const QByteArray value(reinterpret_cast<const char*>(credential->CredentialBlob), int(credential->CredentialBlobSize));
    CredFree(credential);
    return value;
}

void UpdatePanel::OnSaveTokenCallback()
{
    const QByteArray value = m_token->text().trimmed().toUtf8();
    if (value.isEmpty()) {
        if (!CredDeleteW(reinterpret_cast<LPCWSTR>(m_credentialTarget.utf16()), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) {
            m_status->setText(QStringLiteral("Could not remove the saved token."));
            return;
        }
    }
    if (!value.isEmpty()) {
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(m_credentialTarget.utf16()));
        credential.CredentialBlobSize = DWORD(value.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.constData()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (!CredWriteW(&credential, 0)) {
            m_status->setText(QStringLiteral("Windows Credential Manager could not save the token (error %1).").arg(GetLastError()));
            return;
        }
    }
    m_token->clear();
    m_token->setPlaceholderText(value.isEmpty() ? QStringLiteral("GitHub token") : QStringLiteral("Token saved securely; leave blank and save to remove"));
    m_status->setText(value.isEmpty() ? QStringLiteral("Token removed.") : QStringLiteral("Token saved securely. You can now check for updates."));
    m_installerUrl.clear();
    m_download->hide();
    m_install->hide();
}

void UpdatePanel::setBusy(bool busy)
{
    for (auto* button : {m_save, m_check, m_download, m_install}) button->setEnabled(!busy);
    m_token->setEnabled(!busy);
}

void UpdatePanel::OnCheckCallback()
{
    if (m_reply) return;
    if (token().isEmpty()) {
        m_status->setText(QStringLiteral("Save a GitHub token with access to the private repository to discover updates."));
        return;
    }
    m_download->hide();
    m_install->hide();
    m_installerUrl.clear();
    m_status->setText(QStringLiteral("Checking GitHub releases…"));
    setBusy(true);
    QNetworkRequest request(m_apiUrl);
    request.setRawHeader("User-Agent", "MediaSuite-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("Authorization", "Bearer " + token());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_reply.data();
        m_reply = nullptr;
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            m_status->setText(QStringLiteral("Update check failed (HTTP %1). Check the token, repository permission, and connection.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
            return;
        }
        const auto release = QJsonDocument::fromJson(reply->readAll()).object();
        QString version = release.value(QStringLiteral("tag_name")).toString();
        if (version.startsWith(QLatin1Char('v'))) version.remove(0, 1);
        if (!QRegularExpression(QStringLiteral("^[0-9]+\\.[0-9]+\\.[0-9]+$")).match(version).hasMatch() || release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool()) {
            m_status->setText(QStringLiteral("No valid stable release was found."));
            return;
        }
        if (QVersionNumber::fromString(version) <= QVersionNumber::fromString(QCoreApplication::applicationVersion())) {
            m_status->setText(QStringLiteral("Media Suite is up to date (%1).").arg(QCoreApplication::applicationVersion()));
            return;
        }
        for (const auto& item : release.value(QStringLiteral("assets")).toArray()) {
            const auto asset = item.toObject();
            if (asset.value(QStringLiteral("name")).toString() != QStringLiteral("MediaSuite-%1-setup.exe").arg(version) || asset.value(QStringLiteral("state")).toString() != QStringLiteral("uploaded")) continue;
            const QString digest = asset.value(QStringLiteral("digest")).toString();
            const QUrl url(asset.value(QStringLiteral("url")).toString());
            if (!QRegularExpression(QStringLiteral("^sha256:[0-9a-fA-F]{64}$")).match(digest).hasMatch() || url.scheme() != m_apiUrl.scheme() || url.host() != m_apiUrl.host() || url.port() != m_apiUrl.port() || !url.path().startsWith(m_apiUrl.path().section(QStringLiteral("/releases"), 0, 0) + QStringLiteral("/releases/assets/"))) continue;
            m_version = version;
            m_expectedHash = digest.mid(7).toLatin1().toLower();
            m_expectedSize = qint64(asset.value(QStringLiteral("size")).toDouble());
            if (m_expectedSize <= 0 || m_expectedSize > 2LL * 1024 * 1024 * 1024) continue;
            m_installerUrl = url;
            m_download->show();
            m_status->setText(QStringLiteral("Media Suite %1 is available. Download it to update.").arg(version));
            emit updateAvailable(version);
            return;
        }
        m_status->setText(QStringLiteral("A newer release exists, but it has no verified Windows installer."));
    });
}

void UpdatePanel::OnDownloadCallback()
{
    if (m_reply || m_installerUrl.isEmpty()) return;
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/updates/") + m_version;
    if (!QDir().mkpath(folder)) {
        m_status->setText(QStringLiteral("Could not create the update download folder."));
        return;
    }
    m_readyInstaller = folder + QStringLiteral("/MediaSuite-%1-setup.exe").arg(m_version);
    m_output = std::make_unique<QSaveFile>(m_readyInstaller);
    if (!m_output->open(QIODevice::WriteOnly)) {
        m_status->setText(QStringLiteral("Could not open the update download file."));
        return;
    }
    m_hash.reset();
    m_receivedSize = 0;
    m_redirects = 0;
    m_install->hide();
    setBusy(true);
    startInstallerDownload(m_installerUrl);
}

void UpdatePanel::startInstallerDownload(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MediaSuite-Updater");
    request.setRawHeader("Accept", "application/octet-stream");
    if (url.host() == m_apiUrl.host() && url.port() == m_apiUrl.port() && url.scheme() == m_apiUrl.scheme()) request.setRawHeader("Authorization", "Bearer " + token());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(60000);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status < 200 || status >= 300) return;
        const QByteArray data = m_reply->readAll();
        m_receivedSize += data.size();
        if (m_receivedSize > m_expectedSize || m_output->write(data) != data.size()) {
            m_reply->abort();
            return;
        }
        m_hash.addData(data);
        m_status->setText(QStringLiteral("Downloading update %1 · %2%").arg(m_version).arg(m_receivedSize * 100 / m_expectedSize));
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto* reply = m_reply.data();
        m_reply = nullptr;
        reply->deleteLater();
        const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!redirect.isEmpty()) {
            const QUrl next = reply->url().resolved(redirect);
            const bool sameOrigin = next.scheme() == m_apiUrl.scheme() && next.host() == m_apiUrl.host() && next.port() == m_apiUrl.port();
            if (++m_redirects <= 5 && (sameOrigin || (next.scheme() == QStringLiteral("https") && next.host().endsWith(QStringLiteral(".githubusercontent.com"))))) {
                startInstallerDownload(next);
                return;
            }
        }
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError || !redirect.isEmpty() || m_receivedSize != m_expectedSize || m_hash.result().toHex() != m_expectedHash) {
            m_output->cancelWriting();
            m_output.reset();
            m_status->setText(QStringLiteral("Update download failed verification or was interrupted. You can retry."));
            return;
        }
        if (!m_output->commit()) {
            m_status->setText(QStringLiteral("Could not save the verified installer."));
            return;
        }
        m_output.reset();
        m_download->hide();
        m_install->show();
        m_status->setText(QStringLiteral("Update %1 downloaded and SHA-256 verified. Install when your jobs are finished.").arg(m_version));
    });
}
