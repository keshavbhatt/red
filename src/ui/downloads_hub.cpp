#include "ui/downloads_hub.h"

#include "core/downloads/download_queue.h"
#include "core/notifications/notification_service.h"
#include "core/settings/settings.h"
#include "core/youtube_url.h"
#include "platform/file_manager.h"
#include "services/engine_manager.h"
#include "services/media_probe.h"
#include "ui/download_dialog.h"
#include "ui/downloads_panel.h"
#include "ui/engine_setup_dialog.h"
#include "ui/logging.h"
#include "ui/thumbnail_cache.h"
#include "web/cookie_exporter.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QTemporaryFile>
#include <QTimer>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace red::ui {

DownloadsHub::DownloadsHub(core::Settings& settings, core::ThemeService& theme,
                           core::NotificationService& notifications, web::CookieExporter& cookies,
                           QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_notifications(notifications)
    , m_cookies(cookies)
    , m_dialogParent(dialogParent)
    , m_engine(new services::EngineManager(settings, this))
    , m_probe(new services::MediaProbe(this))
    , m_queue(new core::DownloadQueue(this))
    , m_thumbnails(new ThumbnailCache(this))
    , m_persistTimer(new QTimer(this))
{
    m_panel = new DownloadsPanel(*m_queue, settings, theme, *m_thumbnails, dialogParent);
    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(1500);
    connect(m_persistTimer, &QTimer::timeout, this, &DownloadsHub::persist);

    m_queue->load(core::DownloadQueue::defaultFilePath());
    m_queue->setMaxConcurrent(settings.concurrentDownloads());
    connect(&settings, &core::Settings::concurrentDownloadsChanged, m_queue,
            &core::DownloadQueue::setMaxConcurrent);

    auto cookiesProvider = [this]() -> std::unique_ptr<QTemporaryFile> {
        if (!m_settings.useSessionCookies()) {
            return nullptr;
        }
        return m_cookies.writeTempFile();
    };
    m_queue->setCookiesProvider(cookiesProvider);
    m_probe->setCookiesProvider(cookiesProvider);

    wireEngine();
    wireQueue();

    connect(m_panel, &DownloadsPanel::newDownloadRequested, this, &DownloadsHub::promptForUrl);
    connect(m_panel, &DownloadsPanel::cardAction, this, &DownloadsHub::handleCardAction);
    connect(m_panel, &DownloadsPanel::openFolderRequested, this, &DownloadsHub::openDownloadFolder);
    connect(m_panel, &DownloadsPanel::changeFolderRequested, this, &DownloadsHub::changeDownloadFolder);
    connect(m_panel, &DownloadsPanel::engineRequested, this, &DownloadsHub::showEngineDialog);

    connect(&m_notifications, &core::NotificationService::activated, this, [this](quint64 id) {
        const auto it = m_notificationJobs.constFind(id);
        if (it == m_notificationJobs.constEnd()) {
            return;
        }
        if (const auto job = m_queue->job(it.value()); job && !job->primaryFile().isEmpty()) {
            platform::openFile(job->primaryFile());
        } else {
            Q_EMIT showPanelRequested();
        }
    });
    connect(&m_notifications, &core::NotificationService::actionInvoked, this,
            [this](quint64 id, const QString& key) {
                const auto it = m_notificationJobs.constFind(id);
                if (it == m_notificationJobs.constEnd()) {
                    return;
                }
                const auto job = m_queue->job(it.value());
                if (!job) {
                    return;
                }
                if (key == u"folder"_s && !job->primaryFile().isEmpty()) {
                    platform::revealInFileManager(job->primaryFile());
                } else if (key == u"retry"_s) {
                    m_queue->retry(job->id);
                    Q_EMIT showPanelRequested();
                }
            });
    connect(&m_notifications, &core::NotificationService::closed, this,
            [this](quint64 id) { m_notificationJobs.remove(id); });

    m_engine->initialize();
}

DownloadsHub::~DownloadsHub() = default;

void DownloadsHub::wireEngine()
{
    connect(m_engine, &services::EngineManager::statusChanged, m_panel, &DownloadsPanel::setEngineStatus);
    connect(m_engine, &services::EngineManager::ready, this, [this](const core::EnginePaths& paths) {
        m_queue->setEnginePaths(paths);
        m_probe->setEnginePaths(paths);
        const QList<QUrl> pending = m_pendingUrls;
        m_pendingUrls.clear();
        if (!pending.isEmpty() && m_engineDialog) {
            m_engineDialog->accept();
        }
        for (const QUrl& url : pending) {
            openDialog(url);
        }
    });
    connect(m_engine, &services::EngineManager::installFailed, this, [this](const QString& error) {
        qCWarning(lcUi) << "engine install failed:" << error;
        showEngineDialog();
    });
    m_panel->setEngineStatus(m_engine->status());
}

void DownloadsHub::wireQueue()
{
    connect(m_queue, &core::DownloadQueue::changed, this, &DownloadsHub::schedulePersist);
    connect(m_queue, &core::DownloadQueue::activeCountChanged, this, &DownloadsHub::activeCountChanged);
    connect(m_queue, &core::DownloadQueue::jobFinished, this, &DownloadsHub::handleJobFinished);
    connect(m_queue, &core::DownloadQueue::jobAdded, this, [this](quint64) { Q_EMIT showPanelRequested(); });
}

void DownloadsHub::requestDownload(const QUrl& url, bool audioOnly)
{
    if (!core::isDownloadable(url)) {
        qCInfo(lcUi) << "not a downloadable YouTube URL:" << url;
        return;
    }
    if (m_engine->isReady()) {
        openDialog(url, audioOnly);
        return;
    }
    if (audioOnly) {
        m_settings.setLastDownloadKind(core::DownloadKind::Audio); // honoured once the engine is ready
    }
    m_pendingUrls << url;
    showEngineDialog();
    if (!m_engine->status().isBusy()) {
        m_engine->install();
    }
}

void DownloadsHub::openDialog(const QUrl& url, bool audioOnly)
{
    // Ctrl+D can arrive twice (window shortcut + page hotkey): reuse the dialog.
    if (DownloadDialog* existing = m_openDialogs.value(url.toString()); existing != nullptr) {
        if (audioOnly) {
            existing->selectKind(core::DownloadOptions::Kind::Audio);
        }
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto* dialog = new DownloadDialog(url, m_settings, m_theme, *m_probe, *m_thumbnails, m_dialogParent);
    if (audioOnly) {
        dialog->selectKind(core::DownloadOptions::Kind::Audio);
    }
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_openDialogs.insert(url.toString(), dialog);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const quint64 id = m_queue->add(dialog->job());
        qCInfo(lcUi) << "queued download" << id << dialog->job().url;
    });
    dialog->show();
    // Developer aid (headless verification): accept the dialog with its
    // defaults as soon as the probe has filled it in.
    if (qEnvironmentVariableIsSet("RED_DEBUG_AUTODOWNLOAD")) {
        const QString kind = qEnvironmentVariable("RED_DEBUG_AUTODOWNLOAD");
        connect(m_probe, &services::MediaProbe::finished, dialog,
                [dialog, kind](quint64, const core::MediaInfo&) {
                    dialog->selectKind(kind == u"audio"_s      ? core::DownloadOptions::Kind::Audio
                                       : kind == u"advanced"_s ? core::DownloadOptions::Kind::Custom
                                                               : core::DownloadOptions::Kind::Video);
                    if (kind != u"advanced"_s) { // advanced needs a pick; leave it open for inspection
                        QTimer::singleShot(1500, dialog, &QDialog::accept);
                    }
                });
    }
}

void DownloadsHub::promptForUrl()
{
    QString initial;
    const QString clip = QApplication::clipboard()->text().trimmed();
    if (core::isDownloadable(QUrl(clip))) {
        initial = clip;
    }
    bool ok = false;
    const QString text = QInputDialog::getText(m_dialogParent, tr("Download from link"),
                                               tr("Paste a YouTube video, playlist or channel link:"),
                                               QLineEdit::Normal, initial, &ok);
    if (!ok || text.trimmed().isEmpty()) {
        return;
    }
    const QUrl url = QUrl::fromUserInput(text.trimmed());
    if (!core::isDownloadable(url)) {
        QInputDialog::getText(m_dialogParent, tr("Download from link"),
                              tr("That does not look like a YouTube video, playlist or channel link."),
                              QLineEdit::Normal, text.trimmed());
        return;
    }
    requestDownload(url);
}

void DownloadsHub::showEngineDialog()
{
    if (m_engineDialog) {
        m_engineDialog->raise();
        m_engineDialog->activateWindow();
        return;
    }
    m_engineDialog = new EngineSetupDialog(*m_engine, m_theme, m_dialogParent);
    m_engineDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_engineDialog, &QDialog::rejected, this, [this] { m_pendingUrls.clear(); });
    m_engineDialog->show();
}

void DownloadsHub::openDownloadFolder()
{
    const QString dir = m_settings.downloadDirectory();
    QDir().mkpath(dir);
    platform::openDirectory(dir);
}

void DownloadsHub::changeDownloadFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(m_dialogParent, tr("Download folder"),
                                                          m_settings.downloadDirectory());
    if (!dir.isEmpty()) {
        m_settings.setDownloadDirectory(dir);
    }
}

void DownloadsHub::handleCardAction(quint64 jobId, DownloadCardDelegate::Action action)
{
    const auto job = m_queue->job(jobId);
    if (!job) {
        return;
    }
    switch (action) {
    case DownloadCardDelegate::Action::PauseResume:
        if (job->state == core::DownloadState::Paused) {
            m_queue->resume(jobId);
        } else {
            m_queue->pause(jobId);
        }
        break;
    case DownloadCardDelegate::Action::Cancel:
        m_queue->cancel(jobId);
        break;
    case DownloadCardDelegate::Action::Retry:
        m_queue->retry(jobId);
        break;
    case DownloadCardDelegate::Action::Open:
        if (!job->primaryFile().isEmpty()) {
            platform::openFile(job->primaryFile());
        }
        break;
    case DownloadCardDelegate::Action::ShowInFolder:
        if (!job->primaryFile().isEmpty()) {
            platform::revealInFileManager(job->primaryFile());
        } else {
            platform::openDirectory(job->options.outputDirectory);
        }
        break;
    case DownloadCardDelegate::Action::Remove:
        m_queue->remove(jobId);
        break;
    case DownloadCardDelegate::Action::None:
        break;
    }
}

void DownloadsHub::handleJobFinished(quint64 id, core::DownloadState state)
{
    if (!m_settings.notifyOnDownloadFinish() || state == core::DownloadState::Cancelled) {
        return;
    }
    const auto job = m_queue->job(id);
    if (!job) {
        return;
    }
    core::Notification n;
    n.image = m_thumbnails->get(job->thumbnail).toImage();
    if (state == core::DownloadState::Completed) {
        n.title = tr("Download finished");
        n.body = job->title.isEmpty() ? QFileInfo(job->primaryFile()).fileName() : job->title;
        n.category = u"transfer.complete"_s;
        if (!job->primaryFile().isEmpty()) {
            n.actions.append({u"folder"_s, tr("Show in folder")});
        }
    } else {
        n.title = tr("Download failed");
        n.body = (job->title.isEmpty() ? job->url : job->title) + u"\n"_s + job->error;
        n.category = u"transfer.error"_s;
        n.actions.append({u"retry"_s, tr("Retry")});
    }
    const quint64 notificationId = m_notifications.notify(n);
    if (notificationId != 0) {
        m_notificationJobs.insert(notificationId, id);
    }
}

void DownloadsHub::schedulePersist()
{
    m_persistTimer->start();
}

void DownloadsHub::persist()
{
    m_queue->save(core::DownloadQueue::defaultFilePath());
}

void DownloadsHub::shutdown()
{
    m_persistTimer->stop();
    m_probe->cancelAll();
    m_queue->pauseAll();
    persist();
}

} // namespace red::ui
