#pragma once

#include "core/downloads/download_job.h"
#include "ui/download_card_delegate.h"

#include <QDialog>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QTimer;

namespace red::core {
class DownloadQueue;
class NotificationService;
class Settings;
class ThemeService;
} // namespace red::core
namespace red::services {
class EngineManager;
class MediaProbe;
} // namespace red::services
namespace red::web {
class CookieExporter;
}

namespace red::services {
class LicenseService;
}

namespace red::ui {

class DownloadDialog;
class DownloadsPanel;
class EngineSetupDialog;
class ThumbnailCache;

/// Owns everything download-related (FEATURES E1–E13) and wires it to the
/// panel: the engine manager, the probe, the queue, cookies, dialogs and
/// completion notifications. The window only asks it to download a URL or
/// show the panel.
class DownloadsHub : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadsHub)

public:
    DownloadsHub(core::Settings& settings, core::ThemeService& theme,
                 core::NotificationService& notifications, web::CookieExporter& cookies,
                 services::LicenseService& license, QWidget* dialogParent, QObject* parent = nullptr);
    ~DownloadsHub() override;

    [[nodiscard]] DownloadsPanel* panel() { return m_panel; }
    [[nodiscard]] core::DownloadQueue& queue() { return *m_queue; }
    [[nodiscard]] services::EngineManager& engine() { return *m_engine; }
    [[nodiscard]] ThumbnailCache& thumbnails() { return *m_thumbnails; }

    /// Opens the download dialog for a YouTube URL (sets the engine up first if needed).
    /// Opens the download dialog for a video/playlist/channel URL. With
    /// `audioOnly` the dialog opens on "Audio only" (context menu entry).
    void requestDownload(const QUrl& url, bool audioOnly = false);
    /// Asks for a URL (prefilled from the clipboard when it is a YouTube link).
    void promptForUrl();
    void showEngineDialog();
    void openDownloadFolder();
    void changeDownloadFolder();
    /// Called at shutdown: pauses transfers and persists the list.
    void shutdown();

Q_SIGNALS:
    void activeCountChanged(int count);
    void showPanelRequested();
    /// The user queued a download (`alreadyQueued`: the same one was in the
    /// list already, nothing new was added).
    void downloadAdded(const QString& title, bool alreadyQueued);

private:
    void wireEngine();
    void wireQueue();
    void handleCardAction(quint64 jobId, DownloadCardDelegate::Action action);
    void handleJobFinished(quint64 id, core::DownloadState state);
    /// Saves a local copy of the job's thumbnail (ThumbnailCache::keep) as
    /// soon as the picture is available, so the card never depends on YouTube
    /// still serving it (private or removed videos).
    void keepThumbnail(quint64 id);
    void openDialog(const QUrl& url, bool audioOnly = false);
    void schedulePersist();
    void persist();

    core::Settings& m_settings;
    services::LicenseService& m_license;
    core::ThemeService& m_theme;
    core::NotificationService& m_notifications;
    web::CookieExporter& m_cookies;
    QWidget* m_dialogParent;
    services::EngineManager* m_engine = nullptr;
    services::MediaProbe* m_probe = nullptr;
    core::DownloadQueue* m_queue = nullptr;
    ThumbnailCache* m_thumbnails = nullptr;
    DownloadsPanel* m_panel = nullptr;
    QPointer<EngineSetupDialog> m_engineDialog;
    QTimer* m_persistTimer = nullptr;
    struct PendingRequest
    {
        QUrl url;
        bool audioOnly = false;
    };
    QList<PendingRequest> m_pending; ///< waiting for the engine
    QHash<quint64, quint64> m_notificationJobs;
    quint64 m_lastAddedId = 0; ///< set by jobAdded: tells a new job from an ignored duplicate
    QHash<QString, QList<quint64>>
        m_awaitingThumbnail; ///< url → jobs waiting for keep()             ///< notification id → job id
    QHash<QString, QPointer<DownloadDialog>> m_openDialogs; ///< one dialog per URL
};

} // namespace red::ui
