#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QSettings>
#include <QFrame>
#include <QPointer>

namespace magnify::ui { class MainWindow; }
namespace magnify::core { class ConversionJob; }

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
    DownloadCard(QJsonObject request, magnify::ui::MainWindow& converter, QWidget* parent = nullptr);
    ~DownloadCard() override;
    bool active() const;
    bool queued() const;
    bool downloading() const;
    void start();
    void cancel();
signals:
    void stateChanged();
    void importRequested(QString path);
private:
    void OnReadyReadCallback();
    void OnConvertCallback();
    void OnConversionChangedCallback();
    void updateActions();
    QPointer<magnify::ui::MainWindow> m_converter;
    QList<QPointer<magnify::core::ConversionJob>> m_conversions;
    QStringList m_downloadPaths;
    bool m_conversionCancelled = false;
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
    QComboBox* m_outputFormat;
};

class DownloadsView : public QWidget {
    Q_OBJECT
public:
    DownloadsView(magnify::ui::MainWindow& converter, QWidget* parent = nullptr);
    ~DownloadsView() override;
    bool hasActiveDownloads() const;
    void cancelAll();
signals:
    void importRequested(QString path);
private:
    void OnSearchCallback();
    void OnInfoReadyReadCallback();
    void OnDownloadCallback();
    void OnQueueChangedCallback();
    QSettings m_settings{qEnvironmentVariable("MEDIA_SUITE_SETTINGS_ORGANIZATION", "YTDownloader"), QStringLiteral("YTDownloader")};
    QPointer<magnify::ui::MainWindow> m_converter;
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
