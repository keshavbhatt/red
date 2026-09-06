#include "ui/main_window.h"

#include "core/log_sink.h"
#include "core/notifications/notification_service.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "platform/file_manager.h"
#include "platform/notifier_factory.h"
#include "services/engine_manager.h"
#include "services/sponsorblock_client.h"
#include "ui/about_dialog.h"
#include "ui/actions.h"
#include "ui/diagnostics.h"
#include "ui/downloads_hub.h"
#include "ui/downloads_panel.h"
#include "ui/icons.h"
#include "ui/logging.h"
#include "ui/media_controller.h"
#include "ui/permission_prompt.h"
#include "ui/settings_dialog.h"
#include "ui/shortcuts_dialog.h"
#include "ui/side_rail.h"
#include "ui/theme_applier.h"
#include "ui/tray_controller.h"
#include "web/bridge.h"
#include "web/cookie_exporter.h"
#include "web/full_screen_hint.h"
#include "web/permission_controller.h"
#include "web/request_interceptor.h"
#include "web/web_profile.h"
#include "web/web_view.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QSessionManager>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEnginePermission>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kMinimumSize(640, 420);

QSize miniPlayerSize(core::MiniPlayerSize size)
{
    switch (size) {
    case core::MiniPlayerSize::Small:
        return {400, 225};
    case core::MiniPlayerSize::Large:
        return {640, 360};
    case core::MiniPlayerSize::Medium:
        break;
    }
    return {480, 270};
}
} // namespace

MainWindow::MainWindow(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_appVersion(appVersion)
{
    setupUi();
    connectActions();
    connectWebView();
    restoreWindowState();
    applyChrome();

    connect(
        qApp, &QGuiApplication::commitDataRequest, this,
        [this](QSessionManager&) {
            m_quitting = true;
            saveWindowState();
            m_downloads->shutdown();
        },
        Qt::DirectConnection);
}

MainWindow::~MainWindow() = default;

// ---- construction ----------------------------------------------------------

void MainWindow::setupUi()
{
    setWindowTitle(u"Red"_s);
    setWindowIcon(icons::brand());
    resize(kDefaultSize);
    setMinimumSize(kMinimumSize);

    m_themeApplier = new ThemeApplier(m_theme, this);
    m_actions = new Actions(this);
    m_webView = new web::WebView(m_settings, m_theme, m_appVersion, this);
    m_rail = new SideRail(*m_actions, m_theme, this);

    m_notifier = platform::createPlatformNotifier(this).release();
    m_notifications = new core::NotificationService(m_settings, m_notifier, nullptr, this);
    m_downloads =
        new DownloadsHub(m_settings, m_theme, *m_notifications, m_webView->profile().cookies(), this, this);
    m_tray = new TrayController(m_settings, *m_actions, this);
    m_media = new MediaController(m_settings, *m_webView, QApplication::desktopFileName(), this);
    m_hint = new web::FullScreenHint(this);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_rail);
    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    m_splitter->addWidget(m_webView);
    m_splitter->addWidget(m_downloads->panel());
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_downloads->panel()->hide();
    layout->addWidget(m_splitter, 1);
    setCentralWidget(central);

    auto* overflow = new QMenu(this);
    overflow->addAction(m_actions->downloadUrl);
    overflow->addAction(m_actions->fullScreen);
    overflow->addSeparator();
    overflow->addAction(m_actions->zoomIn);
    overflow->addAction(m_actions->zoomOut);
    overflow->addAction(m_actions->zoomReset);
    overflow->addSeparator();
    overflow->addAction(m_actions->shortcuts);
    overflow->addAction(m_actions->openLogFolder);
    overflow->addAction(m_actions->about);
    overflow->addSeparator();
    overflow->addAction(m_actions->quit);
    m_rail->setMenu(overflow);

    m_exitFullScreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_exitFullScreen->setContext(Qt::WindowShortcut);
    m_exitFullScreen->setEnabled(false);
    connect(m_exitFullScreen, &QShortcut::activated, this, [this] {
        if (m_webView->mode() == core::AppMode::Tv) {
            return; // Esc is "back" in Leanback; TV fullscreen leaves via Ctrl+T / F11
        }
        setFullScreenMode(false);
    });

    m_webView->page()->setBackgroundColor(m_theme.isDark() ? QColor(0x0F, 0x0F, 0x0F)
                                                           : QColor(0xFF, 0xFF, 0xFF));
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) {
        m_webView->page()->setBackgroundColor(m_theme.isDark() ? QColor(0x0F, 0x0F, 0x0F)
                                                               : QColor(0xFF, 0xFF, 0xFF));
    });
}

void MainWindow::connectActions()
{
    Actions& a = *m_actions;
    connect(a.home, &QAction::triggered, this, [this] { m_webView->goHome(); });
    connect(a.back, &QAction::triggered, this, [this] { m_webView->back(); });
    connect(a.forward, &QAction::triggered, this, [this] { m_webView->forward(); });
    connect(a.reload, &QAction::triggered, this, [this] {
        if (m_webView->page()->isLoading()) {
            m_webView->stop();
        } else {
            m_webView->reload();
        }
    });
    connect(a.download, &QAction::triggered, this, [this] {
        const QUrl url = m_webView->downloadableUrl();
        if (url.isEmpty()) {
            m_downloads->promptForUrl();
        } else {
            m_downloads->requestDownload(url);
        }
    });
    connect(a.downloadUrl, &QAction::triggered, m_downloads, &DownloadsHub::promptForUrl);
    connect(a.downloads, &QAction::toggled, this, &MainWindow::setDownloadsPanelVisible);
    connect(a.miniPlayer, &QAction::toggled, this, &MainWindow::setMiniPlayer);
    connect(a.tvMode, &QAction::toggled, this, &MainWindow::setTvMode);
    connect(a.playPause, &QAction::triggered, m_media, &MediaController::playPause);
    connect(a.next, &QAction::triggered, m_media, &MediaController::next);
    connect(a.previous, &QAction::triggered, m_media, &MediaController::previous);
    connect(a.zoomIn, &QAction::triggered, this, [this] { m_webView->zoomStep(1); });
    connect(a.zoomOut, &QAction::triggered, this, [this] { m_webView->zoomStep(-1); });
    connect(a.zoomReset, &QAction::triggered, this, [this] { m_webView->zoomReset(); });
    connect(a.fullScreen, &QAction::toggled, this, &MainWindow::setFullScreenMode);
    connect(a.showHide, &QAction::triggered, this, &MainWindow::toggleVisibility);
    connect(a.settings, &QAction::triggered, this, &MainWindow::showSettings);
    connect(a.shortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    connect(a.openLogFolder, &QAction::triggered, this,
            [] { platform::openDirectory(QFileInfo(core::LogSink::logFilePath()).absolutePath()); });
    connect(a.about, &QAction::triggered, this, &MainWindow::showAbout);
    connect(a.quit, &QAction::triggered, this, &MainWindow::quit);

    connect(m_tray, &TrayController::toggleRequested, this, &MainWindow::toggleVisibility);
    connect(m_media, &MediaController::stateChanged, m_tray, &TrayController::setMediaState);
    connect(m_media, &MediaController::raiseRequested, this, &MainWindow::showAndRaise);
    connect(m_downloads, &DownloadsHub::activeCountChanged, m_rail, &SideRail::setActiveDownloads);
    connect(m_downloads, &DownloadsHub::showPanelRequested, this, [this] {
        if (!m_panelVisible && !isFullScreen() && !m_miniPlayer &&
            m_webView->mode() == core::AppMode::Desktop) {
            m_actions->downloads->setChecked(true);
        }
    });
    connect(m_downloads->panel(), &DownloadsPanel::closeRequested, this,
            [this] { m_actions->downloads->setChecked(false); });
    connect(&m_settings, &core::Settings::showRailChanged, this, [this](bool) { applyChrome(); });
    connect(&m_settings, &core::Settings::closeActionChanged, this,
            [this](core::CloseAction) { QApplication::setQuitOnLastWindowClosed(false); });
    m_tray->setMediaState({});
}

void MainWindow::connectWebView()
{
    connect(m_webView, &web::WebView::fullScreenRequested, this,
            [this](bool on) { m_actions->fullScreen->setChecked(on); });
    connect(m_webView, &web::WebView::renderProcessGaveUp, this, &MainWindow::handleRenderProcessGaveUp);
    connect(m_webView, &web::WebView::permissionPromptRequested, this, &MainWindow::handlePermissionPrompt);
    connect(m_webView, &web::WebView::downloadRequested, m_downloads, &DownloadsHub::requestDownload);
    connect(m_webView, &web::WebView::downloadableChanged, m_rail, &SideRail::setDownloadable);
    connect(m_webView, &web::WebView::tvSignal, this, &MainWindow::handleTvSignal);
    connect(m_webView->page(), &QWebEnginePage::loadingChanged, this,
            [this](const QWebEngineLoadingInfo&) { m_rail->setLoading(m_webView->page()->isLoading()); });
    connect(m_webView->page(), &QWebEnginePage::titleChanged, this, [this](const QString& title) {
        const QString clean = QString(title).remove(u" - YouTube"_s).remove(u" - YouTube TV"_s).trimmed();
        setWindowTitle(clean.isEmpty() || clean == u"YouTube"_s ? u"Red"_s : clean + u" — Red"_s);
    });
    connect(m_webView->page(), &QWebEnginePage::urlChanged, this, [this](const QUrl&) {
        m_actions->back->setEnabled(m_webView->history()->canGoBack());
        m_actions->forward->setEnabled(m_webView->history()->canGoForward());
    });
    // SponsorBlock: the page asks, the client answers through the bridge (ADR-007).
    auto* sponsor = new services::SponsorBlockClient(m_settings, this);
    connect(m_webView, &web::WebView::sponsorSegmentsRequested, sponsor,
            &services::SponsorBlockClient::request);
    connect(sponsor, &services::SponsorBlockClient::segmentsReady, &m_webView->profile().bridge(),
            &web::Bridge::sponsorSegments);
}

// ---- lifecycle -------------------------------------------------------------

void MainWindow::start(bool tvMode)
{
    m_startedInTv = tvMode;
    const bool tv = tvMode || m_settings.tvStartInTvMode() || m_settings.lastMode() == core::AppMode::Tv;
    if (tv) {
        m_actions->tvMode->setChecked(true); // switches the profile, loads Leanback
    } else {
        m_webView->loadStart();
    }
    if (m_settings.downloadsPanelOpen() && !tv) {
        m_actions->downloads->setChecked(true);
    }
    show();
    QTimer::singleShot(600, this, &MainWindow::maybeShowGpuFallbackNotice);
}

void MainWindow::restoreWindowState()
{
    const QByteArray geometry = m_settings.windowGeometry();
    if (!geometry.isEmpty() && !restoreGeometry(geometry)) {
        qCWarning(lcUi) << "stored window geometry rejected, using defaults";
    }
    const QByteArray state = m_settings.windowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
}

void MainWindow::saveWindowState()
{
    if (m_miniPlayer) {
        m_settings.setWindowGeometry(m_geometryBeforeMini);
        return;
    }
    if (isFullScreen()) {
        if (!m_geometryBeforeFullScreen.isEmpty()) {
            m_settings.setWindowGeometry(m_geometryBeforeFullScreen);
        }
        return;
    }
    m_settings.setWindowGeometry(saveGeometry());
    m_settings.setWindowState(saveState());
    m_settings.setDownloadsPanelOpen(m_panelVisible);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_quitting && m_settings.closeAction() == core::CloseAction::MinimizeToTray &&
        m_tray->isAvailable()) {
        event->ignore();
        hide();
        return;
    }
    m_quitting = true;
    saveWindowState();
    m_downloads->shutdown();
    m_settings.setBlockedCount(m_settings.blockedCount() + m_webView->profile().interceptor().blockedCount());
    m_webView->profile().interceptor().resetBlockedCount();
    event->accept();
    QApplication::quit();
}

void MainWindow::quit()
{
    m_quitting = true;
    close();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        const bool fs = isFullScreen();
        if (m_actions->fullScreen->isChecked() != fs) {
            const QSignalBlocker blocker(m_actions->fullScreen);
            m_actions->fullScreen->setChecked(fs);
            m_exitFullScreen->setEnabled(fs);
            applyChrome();
        }
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_tray->setWindowVisible(true);
}

void MainWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    m_tray->setWindowVisible(false);
}

void MainWindow::showAndRaise()
{
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::toggleVisibility()
{
    if (isVisible() && !isMinimized()) {
        if (!m_tray->isAvailable()) {
            showMinimized();
        } else {
            hide();
        }
    } else {
        showAndRaise();
    }
}

// ---- chrome ----------------------------------------------------------------

void MainWindow::applyChrome()
{
    const bool tv = m_webView->mode() == core::AppMode::Tv;
    const bool bare = isFullScreen() || m_miniPlayer || tv;
    m_rail->setVisible(!bare && m_settings.showRail());
    m_downloads->panel()->setVisible(!bare && m_panelVisible);
    m_actions->downloads->setEnabled(!tv && !m_miniPlayer);
    m_actions->miniPlayer->setEnabled(!tv);
}

void MainWindow::setDownloadsPanelVisible(bool visible)
{
    m_panelVisible = visible;
    applyChrome();
}

void MainWindow::setFullScreenMode(bool on)
{
    if (!on) {
        m_hint->hideHint();
    }
    if (on == isFullScreen()) {
        m_exitFullScreen->setEnabled(on);
        if (!on) {
            m_webView->exitPageFullScreen();
        }
        applyChrome();
        return;
    }
    if (on) {
        m_stateBeforeFullScreen = windowState();
        m_geometryBeforeFullScreen = saveGeometry();
        showFullScreen();
        m_exitFullScreen->setEnabled(true);
        if (m_webView->mode() != core::AppMode::Tv) {
            m_hint->showHint(tr("Press Esc or F11 to exit full screen"));
        }
    } else {
        m_exitFullScreen->setEnabled(false);
        if (m_stateBeforeFullScreen & Qt::WindowMaximized) {
            showMaximized();
        } else {
            showNormal();
        }
        m_webView->exitPageFullScreen();
    }
    applyChrome();
}

void MainWindow::setTvMode(bool on)
{
    const core::AppMode mode = on ? core::AppMode::Tv : core::AppMode::Desktop;
    if (m_webView->mode() == mode && m_webView->url().isValid() && !m_webView->url().isEmpty()) {
        return;
    }
    if (on && m_miniPlayer) {
        m_actions->miniPlayer->setChecked(false);
    }
    m_webView->setMode(mode);
    applyChrome();
    if (on) {
        if (m_settings.tvFullscreen() && !isFullScreen()) {
            m_actions->fullScreen->setChecked(true);
        }
        m_hint->showHint(tr("TV mode  ·  Esc: back  ·  Ctrl+T: desktop  ·  Ctrl+D: download"));
        qCInfo(lcUi) << "entered TV mode";
    } else {
        m_hint->hideHint();
        if (isFullScreen() && m_settings.tvFullscreen()) {
            m_actions->fullScreen->setChecked(false);
        }
        qCInfo(lcUi) << "left TV mode";
    }
}

void MainWindow::setMiniPlayer(bool on)
{
    if (on == m_miniPlayer) {
        return;
    }
    m_miniPlayer = on;
    if (on) {
        if (isFullScreen()) {
            m_actions->fullScreen->setChecked(false);
        }
        m_geometryBeforeMini = saveGeometry();
        m_stateBeforeMini = windowState();
        applyChrome();
        m_webView->setMiniPlayer(true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setMinimumSize(320, 180);
        showNormal();
        resize(miniPlayerSize(m_settings.miniPlayerSize()));
        show();
        m_hint->showHint(tr("Mini player  ·  Ctrl+Shift+M to restore"));
        qCInfo(lcUi) << "mini player on";
    } else {
        m_webView->setMiniPlayer(false);
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
        setMinimumSize(kMinimumSize);
        show();
        if (!m_geometryBeforeMini.isEmpty()) {
            restoreGeometry(m_geometryBeforeMini);
        }
        if (m_stateBeforeMini & Qt::WindowMaximized) {
            showMaximized();
        }
        applyChrome();
        m_hint->hideHint();
        qCInfo(lcUi) << "mini player off";
    }
}

// ---- commands --------------------------------------------------------------

void MainWindow::openUrl(const QString& url)
{
    const QUrl target = QUrl::fromUserInput(url);
    if (!target.isValid() || !core::isYouTubeHost(target.host())) {
        qCWarning(lcUi) << "ignoring non-YouTube url" << url;
        return;
    }
    showAndRaise();
    m_webView->openYouTubeUrl(target);
}

void MainWindow::downloadUrl(const QString& url)
{
    showAndRaise();
    m_downloads->requestDownload(QUrl::fromUserInput(url));
}

void MainWindow::handleTvSignal(const QString& signal, const QString&)
{
    if (signal == u"exit"_s) {
        if (m_startedInTv) {
            quit();
        } else {
            m_actions->tvMode->setChecked(false);
        }
    } else if (signal == u"reload"_s) {
        m_webView->reload();
    }
}

QString MainWindow::engineSummary() const
{
    const services::EngineManager::Status& s = m_downloads->engine().status();
    return u"yt-dlp %1, js %2, ffmpeg %3"_s.arg(s.ytdlpVersion.isEmpty() ? u"missing"_s : s.ytdlpVersion,
                                                s.jsRuntime.isEmpty() ? u"missing"_s : s.jsRuntime,
                                                s.ffmpegPath.isEmpty() ? u"missing"_s : s.ffmpegPath);
}

void MainWindow::showSettings()
{
    if (m_settingsDialog) {
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
        return;
    }
    m_settingsDialog = new SettingsDialog(m_settings, m_theme, m_downloads->engine(),
                                          m_webView->profile().interceptor(), m_tray->isAvailable(), this);
    m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_settingsDialog, &SettingsDialog::clearCacheRequested, this, &MainWindow::clearCache);
    connect(m_settingsDialog, &SettingsDialog::clearSessionRequested, this, &MainWindow::confirmClearSession);
    connect(m_settingsDialog, &SettingsDialog::openLogFolderRequested, m_actions->openLogFolder,
            &QAction::trigger);
    connect(m_settingsDialog, &SettingsDialog::copyDiagnosticsRequested, this, &MainWindow::copyDiagnostics);
    connect(m_settingsDialog, &SettingsDialog::engineDialogRequested, m_downloads,
            &DownloadsHub::showEngineDialog);
    m_settingsDialog->show();
}

void MainWindow::showShortcuts()
{
    auto* dialog = new ShortcutsDialog(*m_actions, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showAbout()
{
    auto* dialog = new AboutDialog(m_settings, m_theme, m_webView->userAgent(), engineSummary(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::copyDiagnostics()
{
    QApplication::clipboard()->setText(buildDiagnostics(m_settings, m_webView->userAgent(), engineSummary()));
}

void MainWindow::clearCache()
{
    m_webView->profile().clearHttpCache();
    qCInfo(lcUi) << "http cache cleared";
}

void MainWindow::confirmClearSession()
{
    const auto choice = QMessageBox::warning(
        this, tr("Sign out and clear session?"),
        tr("This removes your YouTube sign-in and all cached data from this computer. Red will quit; "
           "start it again to sign in afresh.\n\nDownloaded files and settings are kept."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) {
        return;
    }
    const QString marker =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/clear-session"_s;
    QFile file(marker);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Cannot clear session"), tr("Could not write %1.").arg(marker));
        return;
    }
    file.close();
    qCInfo(lcUi) << "session clear scheduled for next start";
    quit();
}

void MainWindow::maybeShowGpuFallbackNotice()
{
    if (!m_settings.gpuFallbackNotice()) {
        return;
    }
    m_settings.setGpuFallbackNotice(false);
    QMessageBox::information(
        this, tr("Hardware acceleration turned off"),
        tr("Red hit a graphics-driver problem and switched to software rendering so it stays "
           "stable.\n\nEverything still works; you can turn hardware acceleration back on in "
           "Settings → Advanced."));
}

void MainWindow::handleRenderProcessGaveUp()
{
    showAndRaise();
    const auto choice = QMessageBox::critical(
        this, tr("YouTube keeps crashing"),
        tr("The page's render process has crashed several times in a row.\n\nReload to try again, or quit. "
           "If this keeps happening, try Settings → Advanced → Hardware acceleration: Off."),
        QMessageBox::Retry | QMessageBox::Close, QMessageBox::Retry);
    if (choice == QMessageBox::Retry) {
        m_webView->reload();
    } else {
        quit();
    }
}

void MainWindow::handlePermissionPrompt(const QWebEnginePermission& permission)
{
    askPermission(this, permission,
                  [this, permission](bool allow) { m_webView->permissions().answer(permission, allow); });
}

} // namespace red::ui
