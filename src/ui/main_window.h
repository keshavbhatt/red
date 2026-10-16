#pragma once

#include "core/mini_player_policy.h"
#include "core/settings/settings.h"

#include <QMainWindow>
#include <QPointer>
#include <QUrl>

#include <optional>

class QShortcut;
class QSplitter;
class QWebEnginePermission;

namespace red::core {
class NotificationService;
class INotifier;
class ThemeService;
} // namespace red::core
namespace red::web {
class FullScreenHint;
class WebView;
} // namespace red::web

namespace red::services {
class LicenseService;
}

namespace red::ui {

class Actions;
class DownloadsHub;
class MediaController;
class SettingsDialog;
class SideRail;
class ThemeApplier;
class AccountDialog;
class TrayController;
class OverlayButton;

/// Top-level window (DESIGN.md §2): rail | page | downloads panel. Owns the
/// controllers and implements the window-level behaviours (modes, fullscreen,
/// mini player, close-to-tray). Everything else is delegated.
class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    MainWindow(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
               QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Shows the window in `requested` (--tv / --music), else the remembered mode.
    void start(std::optional<core::AppMode> requested);

public Q_SLOTS:
    void showAndRaise();
    void toggleVisibility();
    void openUrl(const QString& url);
    void downloadUrl(const QString& url);
    /// Switches Desktop / TV / Music (ADR-001, ADR-013); `target` is loaded in the new mode.
    void setMode(core::AppMode mode, std::optional<QUrl> target = std::nullopt);
    void showSettings();
    void showShortcuts();
    void showAbout();
    void showAccount();
    /// Headless verification aid (ADR-010): opens a screen by name —
    /// "about", "shortcuts", "account", "link", "whatsnew", "tray" (the tray menu,
    /// popped up over the window).
    void debugOpen(const QString& what);
    void quit();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void connectActions();
    void connectWebView();
    void restoreWindowState();
    void saveWindowState();
    void maybeShowGpuFallbackNotice();
    /// Release notes once per version; `force` shows them regardless.
    void maybeShowWhatsNew(bool force = false);
    void setFullScreenMode(bool on);
    void setMiniPlayer(bool on);
    void setDownloadsPanelVisible(bool visible);
    void applyChrome();
    void syncModeActions();
    void showWelcome();
    void fitMiniPlayer();
    void placeDownloadsPanel();
    void announceDownload(const QString& title, bool alreadyQueued);
    void requestDownload(const QUrl& url);
    void applyPageBackground();
    void handleRenderProcessGaveUp();
    void handlePermissionPrompt(const QWebEnginePermission& permission);
    void installScriptDialogs();
    void promptUpgrade(const QString& feature);
    void promptDownloadLimit(int requested, int remaining);
    void handleTvSignal(const QString& signal, const QString& argument);
    void clearCache();
    void confirmClearSession();
    void copyDiagnostics();
    [[nodiscard]] QString engineSummary() const;

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_appVersion;
    web::WebView* m_webView = nullptr;
    Actions* m_actions = nullptr;
    SideRail* m_rail = nullptr;
    QSplitter* m_splitter = nullptr;
    TrayController* m_tray = nullptr;
    services::LicenseService* m_license = nullptr;
    QPointer<AccountDialog> m_accountDialog;
    MediaController* m_media = nullptr;
    ThemeApplier* m_themeApplier = nullptr;
    core::INotifier* m_notifier = nullptr;
    core::NotificationService* m_notifications = nullptr;
    DownloadsHub* m_downloads = nullptr;
    web::FullScreenHint* m_hint = nullptr;
    QShortcut* m_exitFullScreen = nullptr;
    QShortcut* m_toggleTv = nullptr;          ///< Ctrl+T: TV mode on / off
    QShortcut* m_toggleMusic = nullptr;       ///< Ctrl+M: Music mode on / off
    OverlayButton* m_tvExit = nullptr;        ///< "Back to YouTube" in TV mode
    OverlayButton* m_miniRestore = nullptr;   ///< back to the full window from the mini player
    QShortcut* m_exitMiniPlayer = nullptr;    ///< Esc in the mini player
    QShortcut* m_closePanelOverlay = nullptr; ///< Esc while the panel floats over the page
    QMenu* m_overflow = nullptr;              ///< the rail's More menu (F10 opens it without the rail)
    QPointer<SettingsDialog> m_settingsDialog;
    Qt::WindowStates m_stateBeforeFullScreen = Qt::WindowNoState;
    QByteArray m_geometryBeforeFullScreen;
    QByteArray m_geometryBeforeMini;
    core::MiniPlayerShape m_miniShape = core::MiniPlayerShape::Wide;
    Qt::WindowStates m_stateBeforeMini = Qt::WindowNoState;
    bool m_miniPlayer = false;
    bool m_panelVisible = false;
    bool m_quitting = false;
    bool m_startedInTv = false;
};

} // namespace red::ui
