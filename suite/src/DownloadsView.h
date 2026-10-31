#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QSettings>
#include <QFrame>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

class DownloadCard : public QFrame {
    Q_OBJECT
public:
    explicit DownloadCard(QJsonObject request, QWidget* parent = nullptr);
    ~DownloadCard() override;
    bool active() const;
    bool queued() const;
    void start();
    void cancel();
signals:
    void stateChanged();
    void importRequested(QString path);
    void convertRequested(QString path);
private:
    void OnReadyReadCallback();
    QJsonObject m_request;
    QProcess m_process;
    QByteArray m_buffer;
    QString m_state = QStringLiteral("queued");
    QString m_path;
    QLabel* m_status;
    QProgressBar* m_progress;
    QPushButton* m_cancel;
    QPushButton* m_retry;
    QPushButton* m_open;
    QPushButton* m_folder;
    QPushButton* m_import;
    QPushButton* m_convert;
};

class DownloadsView : public QWidget {
    Q_OBJECT
public:
    explicit DownloadsView(QWidget* parent = nullptr);
    ~DownloadsView() override;
    bool hasActiveDownloads() const;
    void cancelAll();
signals:
    void importRequested(QString path);
    void convertRequested(QString path);
private:
    void OnSearchCallback();
    void OnInfoReadyReadCallback();
    void OnDownloadCallback();
    void OnQueueChangedCallback();
    QSettings m_settings{qEnvironmentVariable("MEDIA_SUITE_SETTINGS_ORGANIZATION", "YTDownloader"), QStringLiteral("YTDownloader")};
    QProcess m_probe;
    QByteArray m_probeBuffer;
    QNetworkAccessManager m_network;
    QString m_searchedUrl;
    QLineEdit* m_url;
    QLineEdit* m_directory;
    QLineEdit* m_cookies;
    QComboBox* m_format;
    QComboBox* m_quality;
    QCheckBox* m_playlist;
    QCheckBox* m_autoImport;
    QLabel* m_title;
    QLabel* m_details;
    QLabel* m_thumbnail;
    QLabel* m_message;
    QLabel* m_count;
    QPushButton* m_search;
    QPushButton* m_download;
    QVBoxLayout* m_jobs;
    QList<DownloadCard*> m_cards;
};
