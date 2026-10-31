#include "DownloadsView.h"
#include "Theme.h"
#include "MediaImportService.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

// Both metadata lookup and downloads use the original Python engine.
void startDownloadWorker(QProcess& process)
{
    const QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/worker/download-worker.exe");
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    if (QFileInfo::exists(bundled)) {
        process.start(bundled, {});
        return;
    }
    process.start(qEnvironmentVariable("MEDIA_SUITE_PYTHON", "python"),
                  {QStringLiteral("-u"), QStringLiteral(MEDIA_SUITE_SOURCE_DIR "/suite/download_worker.py")});
}

DownloadCard::DownloadCard(QJsonObject request, QWidget* parent)
    : QFrame(parent), m_request(std::move(request))
{
    setObjectName(QStringLiteral("Card"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    auto* title = new QLabel(m_request.value(QStringLiteral("title")).toString(), this);
    title->setWordWrap(true);
    title->setTextFormat(Qt::PlainText);
    title->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(title);
    m_status = new QLabel(QStringLiteral("Na fila…"), this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setProperty("muted", true);
    layout->addWidget(m_status);
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setValue(0);
    layout->addWidget(m_progress);
    auto* actions = new QHBoxLayout;
    m_cancel = new QPushButton(QStringLiteral("Cancelar"), this);
    m_retry = new QPushButton(QStringLiteral("Tentar novamente"), this);
    m_open = new QPushButton(QStringLiteral("Abrir"), this);
    m_folder = new QPushButton(QStringLiteral("Ver na pasta"), this);
    m_import = new QPushButton(QStringLiteral("Adicionar à biblioteca"), this);
    m_convert = new QPushButton(QStringLiteral("Converter"), this);
    for (auto* button : {m_cancel, m_retry, m_open, m_folder, m_import, m_convert}) {
        actions->addWidget(button);
        button->setVisible(button == m_cancel);
    }
    actions->addStretch();
    layout->addLayout(actions);
    connect(m_cancel, &QPushButton::clicked, this, &DownloadCard::cancel);
    connect(m_retry, &QPushButton::clicked, this, [this] {
        m_state = QStringLiteral("queued");
        m_status->setText(QStringLiteral("Na fila…"));
        m_retry->hide();
        m_cancel->show();
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        emit stateChanged();
    });
    connect(m_open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_path)); });
    connect(m_folder, &QPushButton::clicked, this, [this] {
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(m_path)});
    });
    connect(m_import, &QPushButton::clicked, this, [this] { emit importRequested(m_path); });
    connect(m_convert, &QPushButton::clicked, this, [this] { emit convertRequested(m_path); });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &DownloadCard::OnReadyReadCallback);
    connect(&m_process, &QProcess::started, this, [this] {
        m_process.write(QJsonDocument(m_request).toJson(QJsonDocument::Compact) + '\n');
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_state = QStringLiteral("error");
        m_status->setText(QStringLiteral("Não foi possível iniciar o mecanismo de downloads: %1").arg(m_process.errorString()));
        m_cancel->hide();
        m_retry->show();
        emit stateChanged();
    });
    connect(&m_process, &QProcess::finished, this, [this] {
        OnReadyReadCallback();
        if (m_state == QStringLiteral("running")) {
            m_state = QStringLiteral("error");
            m_status->setText(QStringLiteral("O mecanismo de downloads parou. %1").arg(QString::fromUtf8(m_process.readAllStandardError()).right(1200)));
        }
        m_cancel->hide();
        m_retry->setVisible(m_state != QStringLiteral("done"));
        m_progress->setRange(0, 100);
        emit stateChanged();
    });
}

DownloadCard::~DownloadCard()
{
    if (m_process.state() == QProcess::NotRunning) return;
    m_process.disconnect(this);
    QProcess::execute(QStringLiteral("taskkill.exe"), {QStringLiteral("/PID"), QString::number(m_process.processId()), QStringLiteral("/T"), QStringLiteral("/F")});
    m_process.waitForFinished(3000);
}

bool DownloadCard::active() const
{
    return m_state == QStringLiteral("queued") || m_process.state() != QProcess::NotRunning;
}

bool DownloadCard::queued() const
{
    return m_state == QStringLiteral("queued");
}

void DownloadCard::start()
{
    m_state = QStringLiteral("running");
    m_buffer.clear();
    m_status->setText(QStringLiteral("Conectando…"));
    startDownloadWorker(m_process);
}

void DownloadCard::cancel()
{
    if (!active()) return;
    m_state = QStringLiteral("cancelled");
    m_status->setText(QStringLiteral("Cancelado"));
    m_cancel->hide();
    m_retry->setVisible(m_process.state() == QProcess::NotRunning);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.write("cancel\n");
        QTimer::singleShot(3000, this, [this] {
            if (m_state != QStringLiteral("cancelled") || m_process.state() == QProcess::NotRunning) return;
            QProcess::startDetached(QStringLiteral("taskkill.exe"), {QStringLiteral("/PID"), QString::number(m_process.processId()), QStringLiteral("/T"), QStringLiteral("/F")});
        });
    }
    emit stateChanged();
}

void DownloadCard::OnReadyReadCallback()
{
    m_buffer += m_process.readAllStandardOutput();
    while (m_buffer.contains('\n')) {
        const int end = int(m_buffer.indexOf('\n'));
        const auto event = QJsonDocument::fromJson(m_buffer.left(end)).object();
        m_buffer.remove(0, end + 1);
        if (event.isEmpty() || m_state == QStringLiteral("cancelled")) continue;
        m_status->setText(event.value(QStringLiteral("message")).toString());
        if (event.value(QStringLiteral("type")) == QStringLiteral("progress")) {
            const double percent = event.value(QStringLiteral("percent")).toDouble(-1);
            m_progress->setRange(0, percent < 0 ? 0 : 100);
            m_progress->setValue(int(percent));
            continue;
        }
        m_state = event.value(QStringLiteral("state")).toString();
        if (m_state != QStringLiteral("done")) continue;
        m_path = event.value(QStringLiteral("path")).toString();
        m_progress->setRange(0, 100);
        m_progress->setValue(100);
        const bool exists = QFileInfo::exists(m_path);
        m_open->setVisible(exists);
        m_folder->setVisible(exists);
        const bool video = exists && (QFileInfo(m_path).isDir() || lf::MediaImportService::isVideoFile(m_path));
        m_import->setVisible(video);
        m_convert->setVisible(exists && QFileInfo(m_path).isFile());
        if (video && m_request.value(QStringLiteral("autoImport")).toBool())
            emit importRequested(m_path);
    }
}

DownloadsView::DownloadsView(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* body = new QWidget(scroll);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(18);
    auto* heading = new QLabel(QStringLiteral("Baixar vídeos"), body);
    heading->setObjectName(QStringLiteral("PageTitle"));
    layout->addWidget(heading);
    auto* subtitle = new QLabel(QStringLiteral("Do link à sua biblioteca. Baixe, converta e assista em um só lugar."), body);
    subtitle->setProperty("muted", true);
    layout->addWidget(subtitle);
    auto* link = new QHBoxLayout;
    m_url = new QLineEdit(body);
    m_url->setObjectName(QStringLiteral("downloadUrl"));
    m_url->setPlaceholderText(QStringLiteral("Cole um link: YouTube, Google Drive, outros sites ou arquivo de vídeo direto"));
    auto* paste = new QPushButton(QStringLiteral("Colar"), body);
    m_search = new QPushButton(QStringLiteral("Buscar"), body);
    m_search->setObjectName(QStringLiteral("searchDownload"));
    m_search->setProperty("variant", QStringLiteral("primary"));
    link->addWidget(m_url, 1);
    link->addWidget(paste);
    link->addWidget(m_search);
    layout->addLayout(link);
    auto* preview = new QFrame(body);
    preview->setObjectName(QStringLiteral("Card"));
    auto* previewLayout = new QHBoxLayout(preview);
    previewLayout->setContentsMargins(18, 18, 18, 18);
    m_thumbnail = new QLabel(preview);
    m_thumbnail->setFixedSize(240, 135);
    m_thumbnail->setAlignment(Qt::AlignCenter);
    m_thumbnail->setPixmap(lf::ui::Theme::iconPixmap(lf::ui::Icon::Movie, lf::ui::Colors::accent, 60));
    previewLayout->addWidget(m_thumbnail);
    auto* description = new QVBoxLayout;
    m_title = new QLabel(QStringLiteral("Seus próximos vídeos começam aqui"), preview);
    m_title->setObjectName(QStringLiteral("SectionTitle"));
    m_title->setTextFormat(Qt::PlainText);
    m_title->setWordWrap(true);
    m_details = new QLabel(QStringLiteral("Busque um link para ver a prévia e as qualidades disponíveis."), preview);
    m_details->setProperty("muted", true);
    m_details->setTextFormat(Qt::PlainText);
    m_details->setWordWrap(true);
    description->addWidget(m_title);
    description->addWidget(m_details);
    description->addStretch();
    previewLayout->addLayout(description, 1);
    layout->addWidget(preview);
    auto* options = new QHBoxLayout;
    m_format = new QComboBox(body);
    m_format->addItems({QStringLiteral("Vídeo · melhor disponível"), QStringLiteral("Áudio · MP3")});
    m_format->setCurrentIndex(m_settings.value(QStringLiteral("suite/format"), 0).toInt());
    m_quality = new QComboBox(body);
    m_quality->addItem(QStringLiteral("Melhor qualidade"), 0);
    m_playlist = new QCheckBox(QStringLiteral("Baixar a playlist inteira"), body);
    options->addWidget(m_format);
    options->addWidget(m_quality);
    options->addWidget(m_playlist);
    options->addStretch();
    layout->addLayout(options);
    auto* destination = new QHBoxLayout;
    m_directory = new QLineEdit(m_settings.value(QStringLiteral("out_dir"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString(), body);
    m_directory->setObjectName(QStringLiteral("downloadDirectory"));
    auto* browse = new QPushButton(QStringLiteral("Escolher pasta"), body);
    destination->addWidget(new QLabel(QStringLiteral("Salvar em"), body));
    destination->addWidget(m_directory, 1);
    destination->addWidget(browse);
    layout->addLayout(destination);
    auto* access = new QHBoxLayout;
    m_cookies = new QLineEdit(body);
    m_cookies->setObjectName(QStringLiteral("downloadCookies"));
    m_cookies->setPlaceholderText(QStringLiteral("Opcional: cookies.txt para conteúdos que exigem login"));
    auto* chooseCookies = new QPushButton(QStringLiteral("Escolher cookies"), body);
    access->addWidget(m_cookies, 1);
    access->addWidget(chooseCookies);
    layout->addLayout(access);
    connect(chooseCookies, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Cookies de uma conta com acesso ao conteúdo"), {}, QStringLiteral("Cookies Netscape (*.txt);;Todos os arquivos (*)"));
        if (!path.isEmpty()) m_cookies->setText(path);
    });
    auto* downloadRow = new QHBoxLayout;
    m_autoImport = new QCheckBox(QStringLiteral("Adicionar vídeos à biblioteca ao concluir"), body);
    m_autoImport->setObjectName(QStringLiteral("autoImportDownload"));
    m_autoImport->setChecked(m_settings.value(QStringLiteral("suite/autoImport"), true).toBool());
    m_download = new QPushButton(QStringLiteral("Baixar"), body);
    m_download->setObjectName(QStringLiteral("startDownload"));
    m_download->setProperty("variant", QStringLiteral("primary"));
    m_download->setEnabled(false);
    downloadRow->addWidget(m_autoImport);
    downloadRow->addStretch();
    downloadRow->addWidget(m_download);
    layout->addLayout(downloadRow);
    m_message = new QLabel(body);
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setProperty("muted", true);
    layout->addWidget(m_message);
    auto* queueHeader = new QHBoxLayout;
    m_count = new QLabel(QStringLiteral("Downloads"), body);
    m_count->setObjectName(QStringLiteral("SectionTitle"));
    auto* clear = new QPushButton(QStringLiteral("Limpar concluídos"), body);
    queueHeader->addWidget(m_count);
    queueHeader->addStretch();
    queueHeader->addWidget(clear);
    layout->addLayout(queueHeader);
    m_jobs = new QVBoxLayout;
    m_jobs->setSpacing(12);
    layout->addLayout(m_jobs);
    layout->addStretch();
    scroll->setWidget(body);
    outer->addWidget(scroll);
    connect(paste, &QPushButton::clicked, this, [this] { m_url->setText(QGuiApplication::clipboard()->text().trimmed()); });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Pasta de downloads"), m_directory->text());
        if (!path.isEmpty()) m_directory->setText(path);
    });
    connect(m_format, &QComboBox::currentIndexChanged, this, [this](int index) { m_quality->setEnabled(index == 0); });
    m_quality->setEnabled(m_format->currentIndex() == 0);
    connect(m_url, &QLineEdit::textChanged, this, [this] { m_download->setEnabled(!m_searchedUrl.isEmpty() && m_url->text().trimmed() == m_searchedUrl); });
    connect(m_search, &QPushButton::clicked, this, &DownloadsView::OnSearchCallback);
    connect(m_url, &QLineEdit::returnPressed, this, &DownloadsView::OnSearchCallback);
    connect(m_download, &QPushButton::clicked, this, &DownloadsView::OnDownloadCallback);
    connect(clear, &QPushButton::clicked, this, [this] {
        for (int i = int(m_cards.size()) - 1; i >= 0; --i) {
            if (m_cards[i]->active()) continue;
            delete m_cards.takeAt(i);
        }
        OnQueueChangedCallback();
    });
    connect(&m_probe, &QProcess::started, this, [this] {
        m_probe.write(QJsonDocument(QJsonObject{{QStringLiteral("action"), QStringLiteral("info")}, {QStringLiteral("url"), m_url->text().trimmed()},
            {QStringLiteral("cookies"), m_cookies->text().trimmed()}}).toJson(QJsonDocument::Compact) + '\n');
    });
    connect(&m_probe, &QProcess::readyReadStandardOutput, this, &DownloadsView::OnInfoReadyReadCallback);
    connect(&m_probe, &QProcess::finished, this, [this](int code) {
        OnInfoReadyReadCallback();
        m_search->setEnabled(true);
        if (code != 0 && m_searchedUrl.isEmpty() && m_message->text() == QStringLiteral("Buscando vídeo…"))
            m_message->setText(QStringLiteral("Falha ao buscar. %1").arg(QString::fromUtf8(m_probe.readAllStandardError()).right(1000)));
    });
    connect(&m_probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_search->setEnabled(true);
        m_url->setReadOnly(false);
        m_message->setText(QStringLiteral("Não foi possível iniciar o mecanismo de downloads: %1").arg(m_probe.errorString()));
    });
}

DownloadsView::~DownloadsView()
{
    m_probe.disconnect(this);
    m_probe.kill();
    m_probe.waitForFinished(3000);
    for (auto* card : m_cards) card->disconnect(this);
    qDeleteAll(m_cards);
}

bool DownloadsView::hasActiveDownloads() const
{
    for (auto* card : m_cards)
        if (card->active()) return true;
    return false;
}

void DownloadsView::cancelAll()
{
    // Disconnect scheduling before cancelling queued jobs.
    for (auto* card : m_cards) card->disconnect(this);
    for (auto* card : m_cards) card->cancel();
}

void DownloadsView::OnSearchCallback()
{
    if (m_probe.state() != QProcess::NotRunning) return;
    const QUrl url(m_url->text().trimmed());
    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        m_message->setText(QStringLiteral("Cole um link válido começando com https:// ou http://."));
        return;
    }
    if (!m_cookies->text().trimmed().isEmpty() && !QFileInfo(m_cookies->text().trimmed()).isFile()) {
        m_message->setText(QStringLiteral("O arquivo de cookies não foi encontrado."));
        return;
    }
    m_probeBuffer.clear();
    m_searchedUrl.clear();
    m_download->setEnabled(false);
    m_search->setEnabled(false);
    m_url->setReadOnly(true);
    m_message->setText(QStringLiteral("Buscando vídeo…"));
    connect(&m_probe, &QProcess::finished, m_url, [this] { m_url->setReadOnly(false); }, Qt::SingleShotConnection);
    startDownloadWorker(m_probe);
}

void DownloadsView::OnInfoReadyReadCallback()
{
    m_probeBuffer += m_probe.readAllStandardOutput();
    while (m_probeBuffer.contains('\n')) {
        const int end = int(m_probeBuffer.indexOf('\n'));
        const auto event = QJsonDocument::fromJson(m_probeBuffer.left(end)).object();
        m_probeBuffer.remove(0, end + 1);
        if (event.value(QStringLiteral("type")) != QStringLiteral("info")) {
            if (event.contains(QStringLiteral("message"))) m_message->setText(event.value(QStringLiteral("message")).toString());
            continue;
        }
        m_searchedUrl = m_url->text().trimmed();
        m_title->setText(event.value(QStringLiteral("title")).toString());
        QStringList details{event.value(QStringLiteral("source")).toString()};
        if (!event.value(QStringLiteral("channel")).toString().isEmpty()) details.append(event.value(QStringLiteral("channel")).toString());
        if (event.value(QStringLiteral("duration")).toDouble() > 0) {
            const int duration = event.value(QStringLiteral("duration")).toInt();
            details.append(QStringLiteral("%1:%2").arg(duration / 60).arg(duration % 60, 2, 10, QLatin1Char('0')));
        }
        const bool playlist = event.value(QStringLiteral("playlist")).toBool();
        m_playlist->setChecked(playlist);
        m_playlist->setEnabled(!playlist);
        if (playlist) details.append(event.value(QStringLiteral("count")).toInt() > 0
            ? QStringLiteral("%1 itens").arg(event.value(QStringLiteral("count")).toInt()) : QStringLiteral("Playlist / pasta"));
        m_details->setText(details.join(QStringLiteral(" · ")));
        m_quality->clear();
        m_quality->addItem(QStringLiteral("Melhor qualidade"), 0);
        for (const auto height : event.value(QStringLiteral("heights")).toArray())
            m_quality->addItem(QStringLiteral("%1p").arg(height.toInt()), height.toInt());
        const int saved = m_quality->findData(m_settings.value(QStringLiteral("suite/height"), 0).toInt());
        if (saved >= 0) m_quality->setCurrentIndex(saved);
        m_download->setEnabled(true);
        m_message->clear();
        m_thumbnail->clear();
        m_thumbnail->setPixmap(lf::ui::Theme::iconPixmap(lf::ui::Icon::Movie, lf::ui::Colors::accent, 60));
        const QUrl thumbnail(event.value(QStringLiteral("thumbnail")).toString());
        if (thumbnail.scheme() != QStringLiteral("https") && thumbnail.scheme() != QStringLiteral("http")) continue;
        const QString requestedUrl = m_searchedUrl;
        auto* reply = m_network.get(QNetworkRequest(thumbnail));
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestedUrl] {
            QPixmap image;
            if (requestedUrl == m_searchedUrl && image.loadFromData(reply->readAll()))
                m_thumbnail->setPixmap(image.scaled(m_thumbnail->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            reply->deleteLater();
        });
    }
}

void DownloadsView::OnDownloadCallback()
{
    if (m_searchedUrl.isEmpty() || m_url->text().trimmed() != m_searchedUrl) return;
    const QString directory = m_directory->text().trimmed();
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory) || !QDir().mkpath(directory)) {
        m_message->setText(QStringLiteral("Escolha uma pasta válida para salvar."));
        return;
    }
    m_settings.setValue(QStringLiteral("out_dir"), directory);
    m_settings.setValue(QStringLiteral("suite/format"), m_format->currentIndex());
    m_settings.setValue(QStringLiteral("suite/height"), m_quality->currentData());
    m_settings.setValue(QStringLiteral("suite/autoImport"), m_autoImport->isChecked());
    auto* card = new DownloadCard({
        {QStringLiteral("action"), QStringLiteral("download")}, {QStringLiteral("url"), m_searchedUrl},
        {QStringLiteral("title"), m_title->text()}, {QStringLiteral("directory"), directory},
        {QStringLiteral("audio"), m_format->currentIndex() == 1}, {QStringLiteral("height"), m_quality->currentData().toInt()},
        {QStringLiteral("cookies"), m_cookies->text().trimmed()},
        {QStringLiteral("playlist"), m_playlist->isChecked()}, {QStringLiteral("autoImport"), m_autoImport->isChecked()}}, this);
    m_cards.append(card);
    m_jobs->addWidget(card);
    connect(card, &DownloadCard::stateChanged, this, &DownloadsView::OnQueueChangedCallback);
    connect(card, &DownloadCard::importRequested, this, &DownloadsView::importRequested);
    connect(card, &DownloadCard::convertRequested, this, &DownloadsView::convertRequested);
    OnQueueChangedCallback();
}

void DownloadsView::OnQueueChangedCallback()
{
    int running = 0;
    int active = 0;
    for (auto* card : m_cards) {
        if (card->active()) ++active;
        if (card->active() && !card->queued()) ++running;
    }
    for (auto* card : m_cards) {
        if (running >= 3) break;
        if (!card->queued()) continue;
        card->start();
        ++running;
    }
    m_count->setText(active ? QStringLiteral("Downloads · %1 em andamento").arg(active) : QStringLiteral("Downloads"));
}
