#include "ui/main_window.h"

#include "core/log_sink.h"
#include "core/mini_player_policy.h"
#include "core/notifications/notification_service.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "platform/file_manager.h"
#include "platform/notifier_factory.h"
#include "services/dislikes_client.h"
#include "services/engine_manager.h"
#include "services/licensing/license_service.h"
#include "services/sponsorblock_client.h"
#include "ui/about_dialog.h"
#include "ui/account_dialog.h"
#include "ui/actions.h"
#include "ui/bug_report_dialog.h"
#include "ui/diagnostics.h"
#include "ui/downloads_hub.h"
#include "ui/downloads_panel.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/logging.h"
#include "ui/media_controller.h"
#include "ui/message_sheet.h"
#include "ui/overlay_button.h"
#include "ui/permission_prompt.h"
#include "ui/settings_dialog.h"
#include "ui/shortcuts_dialog.h"
#include "ui/side_rail.h"
#include "ui/theme_applier.h"
#include "ui/tray_controller.h"
#include "ui/welcome_dialog.h"
#include "ui/whats_new_dialog.h"
#include "web/bridge.h"
#include "web/cookie_exporter.h"
#include "web/full_screen_hint.h"
#include "web/permission_controller.h"
#include "web/request_interceptor.h"
#include "web/web_page.h"
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
#include <QResizeEvent>
#include <QSessionManager>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QtEnvironmentVariables>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kMinimumSize(640, 420);
constexpr int kPanelFoldWidth = 900;

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
    installScriptDialogs();
    m_rail = new SideRail(*m_actions, m_theme, this);

    m_notifier = platform::createPlatformNotifier(this).release();
    m_notifications = new core::NotificationService(m_settings, m_notifier, nullptr, this);
    m_license = new services::LicenseService(m_settings, this);
    connect(m_license, &services::LicenseService::upgradeRequested, this, &MainWindow::promptUpgrade);
    connect(m_license, &services::LicenseService::downloadLimitReached, this,
            &MainWindow::promptDownloadLimit);
    m_downloads = new DownloadsHub(m_settings, m_theme, *m_notifications, m_webView->profile().cookies(),
                                   *m_license, this, this);
    m_tray = new TrayController(m_settings, *m_actions, this);
    m_media = new MediaController(m_settings, *m_webView, QApplication::desktopFileName(), this);
    m_hint = new web::FullScreenHint(this);
    m_tvExit = new OverlayButton(this, u"back"_s, tr("Back to YouTube"), OverlayButton::Corner::TopLeft, 24);
    m_tvExit->setToolTip(tr("Leave TV mode  (Ctrl+T)"));
    connect(m_tvExit, &QToolButton::clicked, this, [this] { setMode(core::AppMode::Desktop); });
    m_miniRestore = new OverlayButton(this, u"restore"_s, QString(), OverlayButton::Corner::TopRight, 8);
    m_miniRestore->setToolTip(tr("Back to the full window  (Esc)"));
    m_miniRestore->setAccessibleName(tr("Back to the full window"));
    connect(m_miniRestore, &QToolButton::clicked, this, [this] { m_actions->miniPlayer->setChecked(false); });

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

    auto* overflow = m_overflow = new QMenu(this);
    overflow->addAction(m_actions->downloadUrl);
    overflow->addAction(m_actions->copyLink);
    overflow->addAction(m_actions->fullScreen);
    overflow->addSeparator();
    overflow->addAction(m_actions->zoomIn);
    overflow->addAction(m_actions->zoomOut);
    overflow->addAction(m_actions->zoomReset);
    overflow->addSeparator();
    overflow->addAction(m_actions->shortcuts);
    overflow->addAction(m_actions->welcome);
    overflow->addAction(m_actions->onlineGuide);
    overflow->addAction(m_actions->openLogFolder);
    overflow->addAction(m_actions->account);
    overflow->addAction(m_actions->about);
    overflow->addSeparator();
    overflow->addAction(m_actions->quit);
    m_rail->setMenu(overflow);
    m_rail->setExpanded(m_settings.railLabels());
    connect(m_rail, &SideRail::expandRequested, &m_settings, &core::Settings::setRailLabels);
    connect(&m_settings, &core::Settings::railLabelsChanged, m_rail, &SideRail::setExpanded);

    m_exitFullScreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_exitFullScreen->setContext(Qt::WindowShortcut);
    m_exitFullScreen->setEnabled(false);
    connect(m_exitFullScreen, &QShortcut::activated, this, [this] {
        if (m_webView->mode() == core::AppMode::Tv) {
            return; // Esc is "back" in Leanback; TV fullscreen leaves via Ctrl+T / F11
        }
        setFullScreenMode(false);
    });

    m_closePanelOverlay = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_closePanelOverlay->setContext(Qt::WindowShortcut);
    m_closePanelOverlay->setEnabled(false);
    connect(m_closePanelOverlay, &QShortcut::activated, this,
            [this] { m_actions->downloads->setChecked(false); });

    m_exitMiniPlayer = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_exitMiniPlayer->setContext(Qt::WindowShortcut);
    m_exitMiniPlayer->setEnabled(false); // full screen and the mini player never overlap
    connect(m_exitMiniPlayer, &QShortcut::activated, this,
            [this] { m_actions->miniPlayer->setChecked(false); });

    applyPageBackground();
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyPageBackground(); });
    connect(m_webView, &web::WebView::modeChanged, this, [this](core::AppMode) { applyPageBackground(); });
}

/// The colour behind the page while it loads: YouTube Music is dark-only, so
/// a light theme must not flash white there.
void MainWindow::applyPageBackground()
{
    const bool dark = m_theme.isDark() || m_webView->mode() == core::AppMode::Music;
    m_webView->page()->setBackgroundColor(dark ? QColor(0x0F, 0x0F, 0x0F) : QColor(0xFF, 0xFF, 0xFF));
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
            requestDownload(url);
        }
    });
    connect(a.downloadUrl, &QAction::triggered, m_downloads, &DownloadsHub::promptForUrl);
    connect(a.copyLink, &QAction::triggered, this, [this] {
        // The canonical share link of what is on screen (TV mode: the video being watched).
        const QUrl target =
            m_webView->downloadableUrl().isEmpty() ? m_webView->url() : m_webView->downloadableUrl();
        const std::optional<QUrl> canonical = core::canonicalVideoUrl(target);
        QApplication::clipboard()->setText((canonical ? *canonical : target).toString());
        m_hint->showHint(tr("Link copied"));
    });
    connect(a.downloads, &QAction::toggled, this, &MainWindow::setDownloadsPanelVisible);
    connect(a.miniPlayer, &QAction::toggled, this, &MainWindow::setMiniPlayer);
    // The mode switch selects; picking the mode Red is already in does nothing.
    connect(a.youtubeMode, &QAction::triggered, this, [this] { setMode(core::AppMode::Desktop); });
    connect(a.musicMode, &QAction::triggered, this, [this] { setMode(core::AppMode::Music); });
    connect(a.tvMode, &QAction::triggered, this, [this] { setMode(core::AppMode::Tv); });
    // The older keys stay toggles: Ctrl+T in TV mode (and Ctrl+M in Music) is
    // the way back to YouTube that the hints and the TV controls list name.
    const auto toggle = [this](QKeySequence keys, core::AppMode mode) {
        auto* shortcut = new QShortcut(keys, this);
        shortcut->setContext(Qt::WindowShortcut);
        connect(shortcut, &QShortcut::activated, this,
                [this, mode] { setMode(m_webView->mode() == mode ? core::AppMode::Desktop : mode); });
        return shortcut;
    };
    m_toggleTv = toggle(QKeySequence(Qt::CTRL | Qt::Key_T), core::AppMode::Tv);
    m_toggleMusic = toggle(QKeySequence(Qt::CTRL | Qt::Key_M), core::AppMode::Music);
    connect(a.playPause, &QAction::triggered, m_media, &MediaController::playPause);
    connect(a.next, &QAction::triggered, m_media, &MediaController::next);
    connect(a.previous, &QAction::triggered, m_media, &MediaController::previous);
    connect(a.zoomIn, &QAction::triggered, this, [this] { m_webView->zoomStep(1); });
    connect(a.zoomOut, &QAction::triggered, this, [this] { m_webView->zoomStep(-1); });
    connect(a.zoomReset, &QAction::triggered, this, [this] { m_webView->zoomReset(); });
    connect(a.fullScreen, &QAction::toggled, this, &MainWindow::setFullScreenMode);
    connect(a.showHide, &QAction::triggered, this, &MainWindow::toggleVisibility);
    connect(a.settings, &QAction::triggered, this, &MainWindow::showSettings);
    connect(a.menu, &QAction::triggered, this, [this] {
        if (m_rail->isVisible()) {
            m_rail->showMenu();
        } else {
            m_overflow->popup(mapToGlobal(QPoint(16, 16)));
        }
    });
    connect(a.shortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    connect(a.welcome, &QAction::triggered, this, &MainWindow::showWelcome);
    connect(a.focusNext, &QAction::triggered, this, [this] { cycleFocus(1); });
    m_focusPrevious = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F6), this);
    m_focusPrevious->setContext(Qt::WindowShortcut);
    connect(m_focusPrevious, &QShortcut::activated, this, [this] { cycleFocus(-1); });
    connect(m_rail, &SideRail::leaveRequested, this,
            [this] { m_webView->setFocus(Qt::ShortcutFocusReason); });
    connect(m_downloads->panel(), &DownloadsPanel::leaveRequested, this,
            [this] { m_webView->setFocus(Qt::ShortcutFocusReason); });
    connect(a.onlineGuide, &QAction::triggered, this, [] { platform::openUrl(links::kGuide); });
    connect(a.openLogFolder, &QAction::triggered, this,
            [] { platform::openDirectory(QFileInfo(core::LogSink::logFilePath()).absolutePath()); });
    connect(a.about, &QAction::triggered, this, &MainWindow::showAbout);
    connect(a.account, &QAction::triggered, this, &MainWindow::showAccount);
    connect(a.quit, &QAction::triggered, this, &MainWindow::quit);

    connect(m_tray, &TrayController::toggleRequested, this, &MainWindow::toggleVisibility);
    connect(m_media, &MediaController::stateChanged, m_tray, &TrayController::setMediaState);
    connect(m_media, &MediaController::stateChanged, this,
            [this](const core::MediaState&) { applyChrome(); });
    connect(m_media, &MediaController::raiseRequested, this, &MainWindow::showAndRaise);
    connect(m_downloads, &DownloadsHub::activeCountChanged, m_rail, &SideRail::setActiveDownloads);
    connect(m_downloads, &DownloadsHub::downloadAdded, this, &MainWindow::announceDownload);
    connect(m_downloads, &DownloadsHub::showPanelRequested, this, [this] {
        if (!m_panelVisible && !isFullScreen() && !m_miniPlayer && m_webView->mode() != core::AppMode::Tv) {
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
    connect(m_webView, &web::WebView::downloadRequested, this, &MainWindow::requestDownload);
    connect(m_webView, &web::WebView::audioDownloadRequested, m_downloads,
            [this](const QUrl& url) { m_downloads->requestDownload(url, true); });
    connect(m_webView, &web::WebView::downloadableChanged, m_rail, &SideRail::setDownloadable);
    connect(m_webView, &web::WebView::musicUrlRequested, this, [this](const QUrl& url) {
        setMode(core::AppMode::Music, url);
        showAndRaise();
    });
    connect(m_webView, &web::WebView::tvSignal, this, &MainWindow::handleTvSignal);
    connect(m_webView->page(), &QWebEnginePage::loadingChanged, this,
            [this](const QWebEngineLoadingInfo&) { m_rail->setLoading(m_webView->page()->isLoading()); });
    connect(m_webView->page(), &QWebEnginePage::titleChanged, this, [this](const QString& title) {
        const QString clean = QString(title)
                                  .remove(u" - YouTube Music"_s)
                                  .remove(u" - YouTube TV"_s)
                                  .remove(u" - YouTube"_s)
                                  .trimmed();
        const bool bare = clean.isEmpty() || clean == u"YouTube"_s || clean == u"YouTube Music"_s;
        // The mode in the title too: the task switcher shows where Red is.
        const QString app = m_webView->mode() == core::AppMode::Music ? tr("Red Music")
                            : m_webView->mode() == core::AppMode::Tv  ? tr("Red TV")
                                                                      : u"Red"_s;
        setWindowTitle(bare ? app : clean + u" - "_s + app);
    });
    connect(m_webView->page(), &QWebEnginePage::urlChanged, this, [this](const QUrl&) {
        fitMiniPlayer(); // a Short and a video want different windows
        m_actions->back->setEnabled(m_webView->history()->canGoBack());
        m_actions->forward->setEnabled(m_webView->history()->canGoForward());
    });
    // SponsorBlock: the page asks, the client answers through the bridge (ADR-007).
    auto* sponsor = new services::SponsorBlockClient(m_settings, this);
    connect(m_webView, &web::WebView::sponsorSegmentsRequested, sponsor,
            &services::SponsorBlockClient::request);
    connect(sponsor, &services::SponsorBlockClient::segmentsReady, &m_webView->profile().bridge(),
            &web::Bridge::sponsorSegments);
    auto* dislikes = new services::DislikesClient(m_settings, this);
    connect(m_webView, &web::WebView::dislikesRequested, dislikes, &services::DislikesClient::request);
    connect(dislikes, &services::DislikesClient::dislikesReady, &m_webView->profile().bridge(),
            &web::Bridge::dislikes);
}

// ---- lifecycle -------------------------------------------------------------

void MainWindow::start(std::optional<core::AppMode> requested)
{
    m_startedInTv = requested == core::AppMode::Tv;
    const core::AppMode mode = requested.value_or(m_settings.startupMode());
    if (mode == core::AppMode::Desktop) {
        m_webView->loadStart();
    } else {
        setMode(mode); // switches the profile and loads the mode's start page
    }
    // Reopened only where it docks: a floating panel would cover the page at start.
    if (m_settings.downloadsPanelOpen() && mode != core::AppMode::Tv && width() >= kPanelFoldWidth) {
        m_actions->downloads->setChecked(true);
    }
    show();
    m_webView->setFocus(); // YouTube's own shortcuts (k, j, l, f, m…) need the page focused
    m_license->start();
    // Startup notices run in sequence: the (modal) GPU notices first, then the
    // release notes once those return — and late enough to be past the 3 s
    // Wayland→xcb retry probe in main.cpp, which relaunches the process.
    QTimer::singleShot(600, this, [this] {
        maybeShowGpuFallbackNotice();
        QTimer::singleShot(3000, this, [this] { maybeShowWhatsNew(); });
    });
}

void MainWindow::restoreWindowState()
{
    if (qEnvironmentVariableIsSet("RED_DEBUG_WINDOW_SIZE")) {
        return; // a grab wants the size it asked for, not the remembered one
    }
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
    // The mini player's close button means "back to the big window", not quit
    // (Quit from the menu, the tray or Ctrl+Q still quits).
    if (!m_quitting && m_miniPlayer) {
        event->ignore();
        m_actions->miniPlayer->setChecked(false);
        return;
    }
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

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    placeDownloadsPanel(); // docked or floating follows the width
}

void MainWindow::applyChrome()
{
    const core::AppMode mode = m_webView->mode();
    const bool tv = mode == core::AppMode::Tv;
    const bool music = mode == core::AppMode::Music;
    const bool bare = isFullScreen() || m_miniPlayer || tv;
    m_rail->setVisible(!bare && m_settings.showRail());
    m_tvExit->setActive(tv && !m_miniPlayer);
    placeDownloadsPanel();
    m_actions->downloads->setEnabled(!tv && !m_miniPlayer);
    // The mini player only makes sense with a video on screen (FEATURES D6);
    // while it is on it stays enabled so it can always be turned off. YouTube
    // Music has its own player bar, so it is off there.
    const bool hasVideo = m_media->state().hasMedia() && !m_media->state().videoId.isEmpty();
    // YouTube Music: any playing track (its player page shows the art or video).
    const bool hasPlayer = music ? m_media->state().hasMedia() : hasVideo;
    m_actions->miniPlayer->setEnabled(!tv && (m_miniPlayer || hasPlayer));
    m_actions->miniPlayer->setToolTip(
        m_actions->miniPlayer->isEnabled()
            ? tr("Mini player  (%1)")
                  .arg(m_actions->miniPlayer->shortcut().toString(QKeySequence::NativeText))
        : music ? tr("Play a song to use the mini player")
                : tr("Play a video to use the mini player"));
    // With labels on, the reason sits under the greyed-out label.
    m_rail->setMiniPlayerHint(m_actions->miniPlayer->isEnabled() ? QString()
                              : music                            ? tr("Play a song first")
                                                                 : tr("Play a video first"));
}

/// The downloads panel docks beside the page when the window has room
/// (kPanelFoldWidth); in a narrower window it floats over the page's right
/// side instead, so the Downloads button always shows it. Esc, Ctrl+J or its
/// close button put it away.
void MainWindow::placeDownloadsPanel()
{
    QWidget* panel = m_downloads->panel();
    const bool wanted =
        m_panelVisible && !isFullScreen() && !m_miniPlayer && m_webView->mode() != core::AppMode::Tv;
    const bool docked = width() >= kPanelFoldWidth;
    if (docked && panel->parentWidget() != m_splitter) {
        m_splitter->addWidget(panel);
        m_splitter->setStretchFactor(1, 0);
    } else if (!docked && panel->parentWidget() == m_splitter) {
        panel->setParent(centralWidget()); // out of the splitter, hidden until placed
    }
    const bool floating = wanted && !docked;
    if (floating) {
        const QRect area = centralWidget()->rect();
        panel->setGeometry(area.right() + 1 - panel->width(), area.top(), panel->width(), area.height());
        panel->show();
        panel->raise();
    } else {
        panel->setVisible(wanted && docked);
    }
    m_closePanelOverlay->setEnabled(floating);
}

/// A download was queued: the docked panel shows it; where the panel cannot
/// dock (narrow window, full screen, mini player, TV mode) a hint says so.
void MainWindow::announceDownload(const QString& title, bool alreadyQueued)
{
    const bool bare = isFullScreen() || m_miniPlayer || m_webView->mode() == core::AppMode::Tv;
    const bool docks = !bare && width() >= kPanelFoldWidth;
    if (docks && !m_panelVisible) {
        m_actions->downloads->setChecked(true);
    }
    const bool onScreen = !bare && m_panelVisible;
    if (onScreen && !alreadyQueued) {
        return; // the new card is right there
    }
    const QString name = fontMetrics().elidedText(title.isEmpty() ? tr("your download") : title,
                                                  Qt::ElideRight, std::max(80, width() / 2 - 60));
    QString text =
        alreadyQueued ? tr("Already in your downloads: %1").arg(name) : tr("Download added: %1").arg(name);
    if (!bare && !m_panelVisible) {
        text += tr("  ·  Ctrl+J: downloads");
    }
    m_hint->showHint(text);
}

void MainWindow::cycleFocus(int step)
{
    enum Region
    {
        Page,
        Rail,
        Panel,
    };
    QList<Region> regions{Page};
    if (m_rail->isVisible()) {
        regions << Rail;
    }
    if (m_downloads->panel()->isVisible()) {
        regions << Panel;
    }
    const Region current = m_rail->hasFocusInside()                 ? Rail
                           : m_downloads->panel()->hasFocusInside() ? Panel
                                                                    : Page;
    const qsizetype at = std::max<qsizetype>(0, regions.indexOf(current));
    const qsizetype count = regions.size();
    switch (regions.at((at + step + count) % count)) {
    case Rail:
        m_rail->focusFirst();
        break;
    case Panel:
        m_downloads->panel()->focusFirst();
        break;
    case Page:
        m_webView->setFocus(Qt::ShortcutFocusReason);
        break;
    }
}

void MainWindow::setDownloadsPanelVisible(bool visible)
{
    if (!visible && m_downloads->panel()->hasFocusInside()) {
        m_webView->setFocus(Qt::ShortcutFocusReason); // not lost on a hidden widget
    }
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

void MainWindow::setMode(core::AppMode mode, std::optional<QUrl> target)
{
    const core::AppMode before = m_webView->mode();
    if (before == mode && !target && m_webView->url().isValid() && !m_webView->url().isEmpty()) {
        syncModeActions();
        return;
    }
    if (mode != before && m_miniPlayer) { // each mode has its own mini window
        m_actions->miniPlayer->setChecked(false);
    }
    m_webView->setMode(mode, std::move(target));
    syncModeActions();
    applyChrome();
    if (mode == core::AppMode::Tv) {
        if (m_settings.tvFullscreen() && !isFullScreen()) {
            m_actions->fullScreen->setChecked(true);
        }
        m_hint->showHint(tr("TV mode  ·  Esc: back  ·  Ctrl+T: back to YouTube  ·  Ctrl+D: download"));
    } else {
        if (before == core::AppMode::Tv && isFullScreen() && m_settings.tvFullscreen()) {
            m_actions->fullScreen->setChecked(false);
        }
        if (mode == core::AppMode::Music) {
            m_hint->showHint(tr("YouTube Music  ·  Ctrl+M: back to YouTube  ·  Ctrl+D: download the track"));
        } else {
            m_hint->hideHint();
        }
    }
    qCInfo(lcUi) << "mode:" << core::appModeName(mode);
}

/// The mode switch reflects the current mode without firing.
void MainWindow::syncModeActions()
{
    const core::AppMode mode = m_webView->mode();
    QAction* current = mode == core::AppMode::Tv      ? m_actions->tvMode
                       : mode == core::AppMode::Music ? m_actions->musicMode
                                                      : m_actions->youtubeMode;
    const QSignalBlocker blocker(current);
    current->setChecked(true); // the group unchecks the other two
}

/// Music (the mode, or a YouTube Music link) preselects Audio only.
void MainWindow::requestDownload(const QUrl& url)
{
    const bool audio = m_webView->mode() == core::AppMode::Music || core::isYouTubeMusicHost(url.host());
    m_downloads->requestDownload(url, audio);
}

/// While the mini player is on, a move between a Short and a video (desktop)
/// changes the window between tall and wide.
void MainWindow::fitMiniPlayer()
{
    if (!m_miniPlayer) {
        return;
    }
    const core::MiniPlayerShape shape = core::miniPlayerShape(m_webView->mode(), m_webView->url());
    if (shape == m_miniShape) {
        return;
    }
    m_miniShape = shape;
    resize(core::miniPlayerSize(m_settings.miniPlayerSize(), shape));
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
        setMinimumSize(200, 180); // a Short at Small is 225 px wide
        showNormal();
        m_miniShape = core::miniPlayerShape(m_webView->mode(), m_webView->url());
        resize(core::miniPlayerSize(m_settings.miniPlayerSize(), m_miniShape));
        show();
        // YouTube Music's player page has its menu top right and the (hidden)
        // collapse chevron top left: the restore button takes the chevron's place.
        m_miniRestore->setCorner(m_miniShape == core::MiniPlayerShape::Music
                                     ? OverlayButton::Corner::TopLeft
                                     : OverlayButton::Corner::TopRight);
        m_exitMiniPlayer->setEnabled(true);
        m_miniRestore->setActive(true);           // after the resize: it sits in the small window's corner
        m_hint->showHint(tr("Esc: full window")); // short: a Short's window is 225 px wide
        qCInfo(lcUi) << "mini player on";
    } else {
        m_exitMiniPlayer->setEnabled(false);
        m_miniRestore->setActive(false);
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
    if (core::isYouTubeMusicHost(target.host()) && m_webView->mode() != core::AppMode::Music) {
        setMode(core::AppMode::Music, target);
        return;
    }
    m_webView->openYouTubeUrl(target);
}

void MainWindow::downloadUrl(const QString& url)
{
    showAndRaise();
    if (url.trimmed().isEmpty()) {
        m_downloads->promptForUrl(); // the desktop action "Download from a link…"
        return;
    }
    requestDownload(QUrl::fromUserInput(url.trimmed()));
}

void MainWindow::handleTvSignal(const QString& signal, const QString&)
{
    if (signal == u"exit"_s) {
        if (m_startedInTv) {
            quit();
        } else {
            setMode(core::AppMode::Desktop);
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
    connect(m_settingsDialog, &SettingsDialog::resetPermissionsRequested, this, [this] {
        const int n = m_webView->permissions().resetAll();
        // In the dialog: a hint on the window would sit behind Settings.
        m_settingsDialog->showStorageStatus(n > 0 ? tr("%n site permission(s) reset.", nullptr, n)
                                                  : tr("No site permissions were stored."));
    });
    connect(m_settingsDialog, &SettingsDialog::clearSessionRequested, this, &MainWindow::confirmClearSession);
    connect(m_settingsDialog, &SettingsDialog::openLogFolderRequested, m_actions->openLogFolder,
            &QAction::trigger);
    connect(m_settingsDialog, &SettingsDialog::copyDiagnosticsRequested, this, &MainWindow::copyDiagnostics);
    connect(m_settingsDialog, &SettingsDialog::engineDialogRequested, m_downloads,
            &DownloadsHub::showEngineDialog);
    if (qEnvironmentVariableIsSet("RED_DEBUG_SETTINGS_PAGE")) { // headless verification aid
        m_settingsDialog->showPage(qEnvironmentVariableIntValue("RED_DEBUG_SETTINGS_PAGE"));
    }
    m_settingsDialog->show();
}

void MainWindow::showShortcuts()
{
    auto* dialog = new ShortcutsDialog(*m_actions, m_theme, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::debugOpen(const QString& what)
{
    if (what == u"about"_s) {
        showAbout();
    } else if (what == u"shortcuts"_s) {
        showShortcuts();
    } else if (what == u"account"_s) {
        showAccount();
    } else if (what == u"bug"_s) {
        auto* dialog =
            new BugReportDialog(m_settings, m_theme, m_webView->userAgent(), engineSummary(), this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    } else if (what == u"plans"_s) {
        showAccount();
        m_accountDialog->showPlans();
    } else if (what == u"welcome"_s) {
        showWelcome();
    } else if (what == u"whatsnew"_s) {
        maybeShowWhatsNew(true);
    } else if (what == u"link"_s) {
        m_actions->downloadUrl->trigger();
    } else if (what == u"mini"_s || what == u"mini-close"_s) {
        m_actions->miniPlayer->setChecked(true);
        if (what == u"mini-close"_s) { // the window's close button: back to the full window
            QTimer::singleShot(3000, this, &QWidget::close);
        }
    } else if (what == u"focus"_s) { // F6 once: the rail
        cycleFocus(1);
    } else if (what == u"focus-panel"_s) { // the panel open, F6 twice
        m_actions->downloads->setChecked(true);
        cycleFocus(1);
        cycleFocus(1);
    } else if (what == u"downloads"_s) {
        m_actions->downloads->setChecked(true);
    } else if (what == u"announce"_s) {
        announceDownload(u"Big Buck Bunny 60fps 4K - Official Blender Foundation Short Film"_s, false);
    } else if (what == u"menu"_s) {
        m_rail->showMenu();
    } else if (what == u"tray"_s) {
        if (QMenu* menu = m_tray->menu(); menu != nullptr) {
            menu->popup(mapToGlobal(QPoint(width() / 2, height() / 2)));
        }
    }
}

void MainWindow::showAccount()
{
    if (!m_accountDialog) {
        m_accountDialog = new AccountDialog(*m_license, m_theme, this);
        m_accountDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_accountDialog->show();
    m_accountDialog->raise();
    m_accountDialog->activateWindow();
}

/// A Pro feature was used without a licence: explain once, offer the plans.
void MainWindow::promptUpgrade(const QString& feature)
{
    const QString title =
        m_license->evaluationEnded() ? tr("Your evaluation has ended") : tr("Red Pro feature");
    const QString body =
        m_license->evaluationEnded()
            ? tr("%1 is part of Red Pro, and your %2-day evaluation of the Pro features is over. Get Red Pro "
                 "to keep it, and every other Pro feature.")
                  .arg(feature)
                  .arg(m_license->evaluationDays())
            : tr("%1 is part of Red Pro. Get Red Pro to unlock it, and every other Pro feature.")
                  .arg(feature);
    if (MessageSheet::confirm(this, MessageSheet::Tone::Question, title, body, tr("View plans"),
                              tr("Not now"))) {
        showAccount();
        m_accountDialog->highlightPurchase();
        m_accountDialog->showPlans(); // the comparison on top: what is free, what Pro adds
    }
}

/// The free tier's daily download allowance is used up (or a playlist wants
/// more than is left): explain, offer the plans.
void MainWindow::promptDownloadLimit(int requested, int remaining)
{
    const int limit = services::LicenseService::kFreeDownloadsPerDay;
    QString title;
    QString body;
    if (remaining <= 0) {
        title = tr("Today's free downloads are used up");
        body = tr("Red downloads up to %1 videos a day for free, and today's are used. Get Red Pro for "
                  "unlimited "
                  "downloads, or come back tomorrow.")
                   .arg(limit);
    } else {
        title = tr("Not enough free downloads left today");
        body =
            tr("This playlist has %1 videos, but only %2 of today's %3 free downloads are left. Pick fewer "
               "entries, or get Red Pro for unlimited downloads.")
                .arg(requested)
                .arg(remaining)
                .arg(limit);
    }
    if (MessageSheet::confirm(this, MessageSheet::Tone::Question, title, body, tr("View plans"),
                              tr("Not now"))) {
        showAccount();
        m_accountDialog->highlightPurchase();
        m_accountDialog->showPlans(); // the comparison on top: what is free, what Pro adds
    }
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
    if (!MessageSheet::confirm(this, MessageSheet::Tone::Danger, tr("Sign out and clear session?"),
                               tr("This removes your YouTube sign-in and all cached data from this computer. "
                                  "Red will quit; start it again to sign in afresh. Downloaded files and "
                                  "settings are kept."),
                               tr("Sign out and quit"))) {
        return;
    }
    const QString marker =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/clear-session"_s;
    QFile file(marker);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageSheet::info(this, tr("Cannot clear session"), tr("Could not write %1.").arg(marker));
        return;
    }
    file.close();
    qCInfo(lcUi) << "session clear scheduled for next start";
    quit();
}

void MainWindow::maybeShowGpuFallbackNotice()
{
    if (m_settings.videoDecodeFallbackNotice()) {
        m_settings.setVideoDecodeFallbackNotice(false);
        MessageSheet::info(this, tr("GPU video decoding turned off"),
                           tr("The graphics driver could not decode video on the GPU, so Red switched that "
                              "back off and kept everything else as it was. It usually means no VA-API "
                              "driver is installed for this GPU. You can try again in Settings → Playback."));
        return;
    }
    if (!m_settings.gpuFallbackNotice()) {
        return;
    }
    m_settings.setGpuFallbackNotice(false);
    MessageSheet::info(this, tr("Hardware acceleration turned off"),
                       tr("Red hit a graphics-driver problem and switched to software rendering so it stays "
                          "stable. Everything still works; you can turn hardware acceleration back on in "
                          "Settings → Advanced."));
}

void MainWindow::showWelcome()
{
    // Marked when shown, like What's new: a crash must not loop it.
    m_settings.setWelcomeSeen(true);
    auto* dialog = new WelcomeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &WelcomeDialog::shortcutsRequested, this, &MainWindow::showShortcuts);
    dialog->show();
}

void MainWindow::maybeShowWhatsNew(bool force)
{
    if (!force &&
        (m_settings.whatsNewSeenVersion() == m_appVersion || QApplication::activeModalWidget() != nullptr)) {
        return;
    }
    // A new install gets the welcome instead: release notes mean nothing yet.
    if (!force && m_settings.isFirstRun()) {
        m_settings.setWhatsNewSeenVersion(m_appVersion);
        showWelcome();
        return;
    }
    const QString notes = WhatsNewDialog::bundledNotes(m_appVersion);
    if (notes.isEmpty()) {
        qCWarning(lcUi) << "no changelog section for" << m_appVersion << "- What's new not shown";
        return;
    }
    // Marked when shown, not when closed: a crash must not loop the dialog.
    m_settings.setWhatsNewSeenVersion(m_appVersion);
    auto* dialog = new WhatsNewDialog(m_appVersion, notes, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::handleRenderProcessGaveUp()
{
    showAndRaise();
    const bool reload = MessageSheet::confirm(
        this, MessageSheet::Tone::Warning, tr("YouTube keeps crashing"),
        tr("The page's render process has crashed several times in a row. Reload to try again, or quit. "
           "If this keeps happening, try Settings → Advanced → Hardware acceleration: Off."),
        tr("Reload"), tr("Quit"));
    if (reload) {
        m_webView->reload();
    } else {
        quit();
    }
}

/// The page's alert / confirm / prompt boxes as Red sheets. The "leave this
/// page?" guard is Chromium's own text; it gets Stay / Leave instead of a
/// raw URL in the title bar.
void MainWindow::installScriptDialogs()
{
    web::ScriptDialogs dialogs;
    auto siteName = [](const QUrl& origin) {
        const QString host = origin.host();
        return host.isEmpty() ? tr("This page") : host;
    };
    // `window` is the main window or a pop-up (YouTube Music, sign-in): the
    // sheet belongs to whichever raised it.
    dialogs.alert = [siteName](QWidget* window, const QUrl& origin, const QString& message) {
        MessageSheet::info(window, tr("%1 says").arg(siteName(origin)), message);
    };
    dialogs.confirm = [siteName](QWidget* window, const QUrl& origin, const QString& message) {
        if (message.contains(u"leave this page"_s, Qt::CaseInsensitive)) {
            return MessageSheet::confirm(window, MessageSheet::Tone::Question, tr("Leave this page?"),
                                         tr("Changes you made may not be saved."), tr("Leave"), tr("Stay"));
        }
        return MessageSheet::confirm(window, MessageSheet::Tone::Question,
                                     tr("%1 asks").arg(siteName(origin)), message, tr("OK"));
    };
    dialogs.prompt = [siteName](QWidget* window, const QUrl& origin, const QString& message,
                                const QString& defaultValue, QString* result) {
        bool ok = false;
        const QString text = MessageSheet::askText(window, tr("%1 asks").arg(siteName(origin)), message,
                                                   QString(), defaultValue, tr("OK"), &ok);
        if (ok && result != nullptr) {
            *result = text;
        }
        return ok;
    };
    m_webView->webPage().setScriptDialogs(std::move(dialogs));
}

void MainWindow::handlePermissionPrompt(const QWebEnginePermission& permission)
{
    askPermission(this, permission, [this, permission](bool allow, bool remember) {
        m_webView->permissions().answer(permission, allow, remember);
    });
}

} // namespace red::ui
