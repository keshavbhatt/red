#pragma once

#include "core/settings/settings.h"

#include <QMainWindow>
#include <QPointer>

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

namespace red::ui {

class Actions;
class DownloadsHub;
class MediaController;
class SettingsDialog;
class SideRail;
class ThemeApplier;
class TrayController;

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

    /// Shows the window (honouring the start mode and --tv).
    void start(bool tvMode);

public Q_SLOTS:
    void showAndRaise();
    void toggleVisibility();
    void openUrl(const QString& url);
    void downloadUrl(const QString& url);
    void setTvMode(bool on);
    void showSettings();
    void showShortcuts();
    void showAbout();
    void quit();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupUi();
    void connectActions();
    void connectWebView();
    void restoreWindowState();
    void saveWindowState();
    void maybeShowGpuFallbackNotice();
    void setFullScreenMode(bool on);
    void setMiniPlayer(bool on);
    void setDownloadsPanelVisible(bool visible);
    void applyChrome();
    void handleRenderProcessGaveUp();
    void handlePermissionPrompt(const QWebEnginePermission& permission);
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
    MediaController* m_media = nullptr;
    ThemeApplier* m_themeApplier = nullptr;
    core::INotifier* m_notifier = nullptr;
    core::NotificationService* m_notifications = nullptr;
    DownloadsHub* m_downloads = nullptr;
    web::FullScreenHint* m_hint = nullptr;
    QShortcut* m_exitFullScreen = nullptr;
    QPointer<SettingsDialog> m_settingsDialog;
    Qt::WindowStates m_stateBeforeFullScreen = Qt::WindowNoState;
    QByteArray m_geometryBeforeFullScreen;
    QByteArray m_geometryBeforeMini;
    Qt::WindowStates m_stateBeforeMini = Qt::WindowNoState;
    bool m_miniPlayer = false;
    bool m_panelVisible = false;
    bool m_quitting = false;
    bool m_startedInTv = false;
};

} // namespace red::ui
