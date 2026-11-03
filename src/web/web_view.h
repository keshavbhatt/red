#pragma once

#include "core/media_state.h"
#include "core/render_crash_policy.h"
#include "core/settings/settings.h"
#include "core/swipe_navigation.h"

#include <QElapsedTimer>
#include <QUrl>
#include <QWebEnginePermission>
#include <QWebEngineView>

#include <optional>

class QTimer;
class QWebEngineLoadingInfo;
class QWheelEvent;

namespace red::core {
class ThemeService;
} // namespace red::core

namespace red::web {

class PermissionController;
class SwipeIndicator;
class WebPage;
class WebProfile;

/// The widget that shows YouTube. Owns the profile and page; exposes only
/// what the UI layer needs (mode switching, zoom, media state, download
/// requests, crash recovery, fullscreen, permission prompts).
class WebView : public QWebEngineView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebView)

public:
    WebView(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
            QWidget* parent = nullptr);
    ~WebView() override;

    /// Loads the mode's start page (desktop: per StartPage setting; TV: Leanback).
    void loadStart();
    /// Switches the mode (ADR-001, ADR-013) and reloads: `target` when given,
    /// else the current video carried across, else the mode's start page.
    void setMode(core::AppMode mode, std::optional<QUrl> target = std::nullopt);
    [[nodiscard]] core::AppMode mode() const;
    /// Loads a YouTube URL in the current mode (TV mode maps videos to the Leanback route).
    void openYouTubeUrl(const QUrl& url);
    void goHome();

    void zoomStep(int direction); ///< +1 / -1, persists to settings
    void zoomReset();

    /// Puts the page into / out of mini-player state (FEATURES D6).
    void setMiniPlayer(bool on);
    /// Media control from tray / MPRIS (ADR-007).
    void mediaCommand(const QString& command, double argument = 0);
    [[nodiscard]] const core::MediaState& mediaState() const { return m_media; }
    /// The video/playlist/channel URL the Download button should act on:
    /// the current page (desktop) or the video being watched (TV), or empty.
    [[nodiscard]] QUrl downloadableUrl() const;

    [[nodiscard]] QString userAgent() const;
    [[nodiscard]] WebProfile& profile() { return *m_profile; }
    [[nodiscard]] WebPage& webPage() { return *m_page; }
    [[nodiscard]] PermissionController& permissions() { return *m_permissions; }
    void closePopups();
    void exitPageFullScreen();

Q_SIGNALS:
    void fullScreenRequested(bool on);
    void renderProcessGaveUp();
    void permissionPromptRequested(QWebEnginePermission permission);
    /// The user asked to download this URL (context menu, Ctrl+D in the page, TV).
    void downloadRequested(const QUrl& url);
    void audioDownloadRequested(const QUrl& url);
    void mediaStateChanged(const core::MediaState& state);
    /// SponsorBlock segments wanted for a video (answered via profile().bridge()).
    void sponsorSegmentsRequested(const QString& videoId);
    /// Return YouTube Dislike wanted for a video (answered via profile().bridge()).
    void dislikesRequested(const QString& videoId);
    /// Leanback signals: "exit", "reload", "openUrl"…
    void tvSignal(const QString& signal, const QString& argument);
    void downloadableChanged(bool downloadable);
    void modeChanged(core::AppMode mode);
    /// A new window was asked for a YouTube Music page: it belongs in Music mode.
    void musicUrlRequested(const QUrl& url);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void childEvent(QChildEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// Scroll the page did not use comes back here (two-finger swipe, ADR-022).
    void wheelEvent(QWheelEvent* event) override;

private:
    void applyZoom();
    void wirePopup(QWidget* window);
    void handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int exitCode);
    void handleMediaState(const QJsonObject& state);
    void showLoadError(const QWebEngineLoadingInfo& info);
    void handleUrlChanged(const QUrl& url);
    void updateDownloadable();
    void recoverFromSignInRejection(const QUrl& url);
    [[nodiscard]] QUrl startUrl() const;
    [[nodiscard]] bool swipeAllowed() const;
    void trackSwipe(const QWheelEvent& event);
    void showSwipeProgress();
    void finishSwipe();
    void scheduleDebugSwipes();

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    WebProfile* m_profile = nullptr;
    WebPage* m_page = nullptr;
    PermissionController* m_permissions = nullptr;
    core::RenderCrashPolicy m_crashPolicy;
    core::MediaState m_media;
    QElapsedTimer m_clock;
    QUrl m_hoveredLink;
    bool m_showingError = false;
    bool m_downloadable = false;
    bool m_signInRecoveryTried = false;
    bool m_miniPlayer = false;
    bool m_pageFullScreen = false;
    core::SwipeTracker m_swipe;
    SwipeIndicator* m_swipeIndicator = nullptr;
    QTimer* m_swipeFinish = nullptr;
    QString m_errorTitle;
    QString m_errorDetail;
};

} // namespace red::web
