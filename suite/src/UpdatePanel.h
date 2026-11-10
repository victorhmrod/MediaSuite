#pragma once

#include <QCryptographicHash>
#include <QGroupBox>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QUrl>
#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QNetworkReply;

class UpdatePanel : public QGroupBox {
    Q_OBJECT
public:
    explicit UpdatePanel(QWidget* parent = nullptr,
        QUrl releaseApi = QUrl(QStringLiteral("https://api.github.com/repos/victorhmrod/MediaSuite/releases/latest")),
        QString credentialTarget = QStringLiteral("MediaSuite/GitHubUpdates"));
    ~UpdatePanel() override;
signals:
    void updateAvailable(QString version);
    void installRequested(QString installer);
private:
    void OnSaveTokenCallback();
    void OnCheckCallback();
    void OnDownloadCallback();
    void startInstallerDownload(const QUrl& url);
    void setBusy(bool busy);
    QByteArray token() const;
    QUrl m_apiUrl;
    QString m_credentialTarget;
    QSettings m_settings;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QSaveFile> m_output;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QByteArray m_expectedHash;
    QUrl m_installerUrl;
    QString m_version;
    QString m_readyInstaller;
    qint64 m_expectedSize = 0;
    qint64 m_receivedSize = 0;
    int m_redirects = 0;
    QLineEdit* m_token;
    QLabel* m_status;
    QCheckBox* m_automatic;
    QPushButton* m_save;
    QPushButton* m_check;
    QPushButton* m_download;
    QPushButton* m_install;
};
