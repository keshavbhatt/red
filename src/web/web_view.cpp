#include "web/web_view.h"

#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "core/zoom_policy.h"
#include "platform/file_manager.h"
#include "web/bridge.h"
#include "web/cookie_exporter.h"
#include "web/error_page.h"
#include "web/logging.h"
#include "web/permission_controller.h"
#include "web/popup_window.h"
#include "web/swipe_indicator.h"
#include "web/user_agent.h"
#include "web/web_page.h"
#include "web/web_profile.h"

#include <QAction>
#include <QChildEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMenu>
#include <QTimer>
#include <QWebChannel>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEngineScript>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

namespace red::web {

namespace {
const QUrl kHomeUrl(u"https://www.youtube.com/"_s);
const QUrl kSubscriptionsUrl(u"https://www.youtube.com/feed/subscriptions"_s);
const QUrl kTvUrl(u"https://www.youtube.com/tv"_s);
const QUrl kMusicUrl(u"https://music.youtube.com/"_s);
} // namespace

WebView::WebView(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
                 QWidget* parent)
    : QWebEngineView(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_profile(new WebProfile(settings, appVersion, this))
    , m_page(new WebPage(*m_profile, this))
    , m_permissions(new PermissionController(this))
{
    m_clock.start();
    m_page->setHostWidget(this);
    setPage(m_page);
    applyZoom();

    m_swipeIndicator = new SwipeIndicator(this);
    m_swipeFinish = new QTimer(this);
    m_swipeFinish->setSingleShot(true);
    m_swipeFinish->setInterval(120);
    connect(m_swipeFinish, &QTimer::timeout, this, &WebView::finishSwipe);
    scheduleDebugSwipes();
    connect(&m_settings, &core::Settings::swipeNavigationChanged, this, [this](bool on) {
        if (!on) {
            m_swipe.cancel();
            m_swipeIndicator->setProgress(0);
        }
    });

    // One channel per page exposing the profile's bridge object (ADR-006).
    auto* channel = new QWebChannel(m_page);
    channel->registerObject(u"bridge"_s, &m_profile->bridge());
    m_page->setWebChannel(channel, QWebEngineScript::MainWorld);

    m_permissions->attach(*m_page);
    connect(m_permissions, &PermissionController::promptRequested, this, &WebView::permissionPromptRequested);
    connect(m_page, &WebPage::popupOpened, this, &WebView::wirePopup);
    connect(m_page, &WebPage::musicUrlRequested, this, &WebView::musicUrlRequested);

    connect(&m_settings, &core::Settings::zoomFactorChanged, this, [this](double) { applyZoom(); });
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this, &WebView::handleRenderProcessTerminated);
    connect(m_page, &QWebEnginePage::urlChanged, this, &WebView::handleUrlChanged);
    connect(m_page, &QWebEnginePage::linkHovered, this,
            [this](const QString& url) { m_hoveredLink = QUrl(url); });
    connect(m_page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
            if (url().scheme() != u"data"_s) {
                m_showingError = false;
            }
            m_crashPolicy.onLoadSucceeded();
            if (m_showingError) {
                // an error page is not a place to come back to
            } else if (m_profile->mode() == core::AppMode::Desktop) {
                m_settings.setLastUrl(url().toString());
            } else if (m_profile->mode() == core::AppMode::Music && core::isYouTubeMusicHost(url().host())) {
                m_settings.setLastMusicUrl(url().toString());
            }
        }
    });
    connect(m_page, &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo& info) {
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
            showLoadError(info);
        }
    });
    connect(m_page, &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        request.accept();
        m_pageFullScreen = request.toggleOn();
        Q_EMIT fullScreenRequested(request.toggleOn());
    });

    Bridge& bridge = m_profile->bridge();
    connect(&bridge, &Bridge::retryRequested, this, [this] { loadStart(); });
    connect(&bridge, &Bridge::mediaStateChanged, this, &WebView::handleMediaState);
    connect(&bridge, &Bridge::sponsorSegmentsRequested, this, &WebView::sponsorSegmentsRequested);
    connect(&bridge, &Bridge::dislikesRequested, this, &WebView::dislikesRequested);
    connect(&bridge, &Bridge::tvSignalReceived, this, &WebView::tvSignal);
    connect(&bridge, &Bridge::downloadUrlRequested, this, [this](const QString& url) {
        const QUrl target(url);
        if (core::isDownloadable(target)) {
            Q_EMIT downloadRequested(target);
        }
    });
    connect(&theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) {
        if (m_showingError) {
            m_page->setHtml(errorPageHtml(m_theme.isDark(), m_errorTitle, m_errorDetail));
        }
    });
}

WebView::~WebView()
{
    // Qt WebEngine requires the profile to outlive every page created from it.
    setPage(nullptr);
    for (PopupWindow* popup : findChildren<PopupWindow*>(Qt::FindDirectChildrenOnly)) {
        delete popup;
    }
    delete m_page;
    m_page = nullptr;
}

// ---- navigation ------------------------------------------------------------

QUrl WebView::startUrl() const
{
    if (m_profile->mode() == core::AppMode::Tv) {
        return kTvUrl;
    }
    if (m_profile->mode() == core::AppMode::Music) {
        const QUrl last(m_settings.lastMusicUrl());
        if (m_settings.startPage() == core::StartPage::LastVisited && last.isValid() &&
            core::isYouTubeMusicHost(last.host())) {
            return last;
        }
        return kMusicUrl;
    }
    switch (m_settings.startPage()) {
    case core::StartPage::Subscriptions:
        return kSubscriptionsUrl;
    case core::StartPage::LastVisited: {
        const QUrl last(m_settings.lastUrl());
        if (last.isValid() && core::isYouTubeHost(last.host()) && !core::isYouTubeMusicHost(last.host())) {
            return last;
        }
        return kHomeUrl;
    }
    case core::StartPage::Home:
        break;
    }
    return kHomeUrl;
}

void WebView::loadStart()
{
    load(startUrl());
}

core::AppMode WebView::mode() const
{
    return m_profile->mode();
}

void WebView::setMode(core::AppMode mode, std::optional<QUrl> target)
{
    if (mode == m_profile->mode()) {
        // The profile already starts in the remembered mode; the first
        // "switch" into it at startup still has to load the page.
        if (target) {
            openYouTubeUrl(*target);
        } else if (url().isEmpty()) {
            loadStart();
        }
        return;
    }
    // Carry the video across: a watch page becomes the Leanback route, the
    // YouTube Music track, or the desktop watch page.
    const std::optional<QUrl> current = core::canonicalVideoUrl(url());
    const QString videoId = current ? core::classifyYouTubeUrl(*current).videoId : m_media.videoId;
    m_profile->setMode(mode);
    Q_EMIT modeChanged(mode);
    if (target) {
        openYouTubeUrl(*target);
    } else if (!videoId.isEmpty()) {
        openYouTubeUrl(QUrl(u"https://www.youtube.com/watch?v="_s + videoId));
    } else {
        loadStart();
    }
}

void WebView::openYouTubeUrl(const QUrl& url)
{
    const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(url);
    if (m_profile->mode() == core::AppMode::Tv) {
        if (info.kind == core::YouTubeUrlKind::Video) {
            QUrl tv = kTvUrl;
            tv.setFragment(u"/watch?v="_s + info.videoId);
            load(tv);
            return;
        }
        load(kTvUrl);
        return;
    }
    if (m_profile->mode() == core::AppMode::Music && !core::isYouTubeMusicHost(url.host())) {
        if (info.kind == core::YouTubeUrlKind::Video) {
            load(core::musicVideoUrl(info.videoId, info.playlistId));
            return;
        }
        if (info.kind == core::YouTubeUrlKind::Playlist) {
            load(QUrl(u"https://music.youtube.com/playlist?list="_s + info.playlistId));
            return;
        }
        load(kMusicUrl);
        return;
    }
    load(url);
}

void WebView::goHome()
{
    switch (m_profile->mode()) {
    case core::AppMode::Tv:
        load(kTvUrl);
        return;
    case core::AppMode::Music:
        load(kMusicUrl);
        return;
    case core::AppMode::Desktop:
        break;
    }
    load(kHomeUrl);
}

void WebView::handleUrlChanged(const QUrl& url)
{
    updateDownloadable();
    recoverFromSignInRejection(url);
}

void WebView::updateDownloadable()
{
    const bool downloadable = !downloadableUrl().isEmpty();
    if (downloadable != m_downloadable) {
        m_downloadable = downloadable;
        Q_EMIT downloadableChanged(downloadable);
    }
}

/// Google's "Couldn't sign you in" (embedded-browser check, ADR-008) sticks to
/// the session: once rejected, every later attempt lands on /signin/rejected
/// before the form even shows. Clearing Google's cookies and re-entering the
/// flow once is enough now that the sign-in hosts see a Firefox identity.
void WebView::recoverFromSignInRejection(const QUrl& url)
{
    if (!isGoogleSignInHost(url.host()) || !url.path().startsWith(u"/v3/signin/rejected"_s)) {
        return;
    }
    if (m_signInRecoveryTried) {
        qCWarning(lcWeb) << "Google rejected the sign-in again after a clean retry";
        return;
    }
    m_signInRecoveryTried = true;
    const int removed = m_profile->cookies().deleteCookies(u"google.com"_s);
    qCInfo(lcWeb) << "sign-in rejected by Google; cleared" << removed << "google.com cookies, retrying";
    QTimer::singleShot(300, this, [this] {
        load(QUrl(
            u"https://accounts.google.com/ServiceLogin?service=youtube&continue=https://www.youtube.com/signin"_s));
    });
}

QUrl WebView::downloadableUrl() const
{
    if (m_profile->mode() == core::AppMode::Tv) {
        const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(url());
        const QString id = !info.videoId.isEmpty() ? info.videoId : m_media.videoId;
        return id.isEmpty() ? QUrl() : QUrl(u"https://www.youtube.com/watch?v="_s + id);
    }
    if (m_profile->mode() == core::AppMode::Music) {
        // YouTube Music keeps playing while you browse: the track counts on
        // every page. Playlist and channel pages are offered as themselves.
        const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(url());
        if (info.kind == core::YouTubeUrlKind::Playlist || info.kind == core::YouTubeUrlKind::Channel) {
            return url();
        }
        const QString id = !info.videoId.isEmpty() ? info.videoId : m_media.videoId;
        return id.isEmpty() ? QUrl() : QUrl(u"https://www.youtube.com/watch?v="_s + id);
    }
    // Desktop: only the page itself counts — the player keeps the previous
    // video around after leaving a watch page, which must not be offered.
    return core::isDownloadable(url()) ? url() : QUrl();
}

// ---- zoom ------------------------------------------------------------------

void WebView::applyZoom()
{
    setZoomFactor(m_settings.zoomFactor());
}

void WebView::zoomStep(int direction)
{
    const double current = m_settings.zoomFactor();
    m_settings.setZoomFactor(direction > 0 ? core::zoomIn(current) : core::zoomOut(current));
}

void WebView::zoomReset()
{
    m_settings.setZoomFactor(core::kDefaultZoom);
}

// ---- media / mini player ---------------------------------------------------

void WebView::handleMediaState(const QJsonObject& state)
{
    m_media = core::MediaState::fromJson(state, m_clock.elapsed());
    Q_EMIT mediaStateChanged(m_media);
    updateDownloadable();
}

void WebView::mediaCommand(const QString& command, double argument)
{
    Q_EMIT m_profile->bridge().mediaCommand(command, argument);
}

void WebView::setMiniPlayer(bool on)
{
    m_miniPlayer = on;
    m_profile->setMiniPlayer(on);
}

// ---- misc ------------------------------------------------------------------

QString WebView::userAgent() const
{
    return m_profile->httpUserAgent();
}

void WebView::closePopups()
{
    for (PopupWindow* popup : findChildren<PopupWindow*>(Qt::FindDirectChildrenOnly)) {
        popup->close();
    }
}

void WebView::exitPageFullScreen()
{
    m_page->runJavaScript(u"if (document.fullscreenElement) { document.exitFullscreen(); }"_s,
                          QWebEngineScript::MainWorld);
}

void WebView::wirePopup(QWidget* window)
{
    auto* popup = qobject_cast<PopupWindow*>(window);
    if (popup == nullptr) {
        return;
    }
    m_permissions->attach(*popup->page());
    connect(popup, &PopupWindow::musicUrlRequested, this, &WebView::musicUrlRequested);
}

void WebView::handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status,
                                            int exitCode)
{
    if (status == QWebEnginePage::NormalTerminationStatus) {
        return;
    }
    qCWarning(lcWeb) << "render process terminated, status" << status << "exit code" << exitCode;
    const auto decision = m_crashPolicy.onCrash(std::chrono::milliseconds(m_clock.elapsed()));
    if (!decision.reload) {
        qCCritical(lcWeb) << "render process keeps crashing; asking the user";
        Q_EMIT renderProcessGaveUp();
        return;
    }
    qCInfo(lcWeb) << "reloading in" << decision.delay.count() << "ms";
    QTimer::singleShot(decision.delay, this, [this] { reload(); });
}

void WebView::showLoadError(const QWebEngineLoadingInfo& info)
{
    constexpr int kErrAborted = -3;
    if (info.errorCode() == kErrAborted) {
        return;
    }
    const QString scheme = info.url().scheme();
    if (scheme != u"http"_s && scheme != u"https"_s) {
        return;
    }
    using Domain = QWebEngineLoadingInfo::ErrorDomain;
    QString title;
    QString detail;
    switch (info.errorDomain()) {
    case Domain::ConnectionErrorDomain:
    case Domain::DnsErrorDomain:
        title = tr("You appear to be offline");
        detail = tr("Red couldn't reach YouTube. Check your internet connection, then try again.");
        break;
    case Domain::CertificateErrorDomain:
        title = tr("Secure connection failed");
        detail = tr("The connection to YouTube couldn't be verified. Please try again in a moment.");
        break;
    case Domain::HttpErrorDomain:
        title = tr("YouTube is unavailable");
        detail = tr("YouTube returned an error. Please try again in a moment.");
        break;
    default:
        title = tr("Couldn't load YouTube");
        detail = tr("Something went wrong loading YouTube. Please try again.");
        break;
    }
    qCInfo(lcWeb) << "load failed:" << info.url().toString() << info.errorString() << "-> custom error page";
    m_showingError = true;
    m_errorTitle = title;
    m_errorDetail = detail;
    m_page->setHtml(errorPageHtml(m_theme.isDark(), title, detail));
}

// ---- events ----------------------------------------------------------------

void WebView::childEvent(QChildEvent* event)
{
    QWebEngineView::childEvent(event);
    if (event->type() == QEvent::ChildAdded && event->child() != nullptr) {
        event->child()->installEventFilter(this);
    }
}

bool WebView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
            const int delta = wheel->angleDelta().y();
            if (delta != 0) {
                zoomStep(delta > 0 ? 1 : -1);
            }
            return true;
        }
        trackSwipe(*wheel);
    }
    return QWebEngineView::eventFilter(watched, event);
}

// ---- two-finger swipe (ADR-022) ----------------------------------------------
//
// The touchpad gesture reaches the page through the filter above, with its phases; the
// scroll the page did not use comes back to wheelEvent() without them. Only that unused
// part moves the arrow, so anything that can still scroll sideways keeps the swipe.

bool WebView::swipeAllowed() const
{
    return m_settings.swipeNavigation() && m_profile->mode() != core::AppMode::Tv && !m_miniPlayer &&
           !m_pageFullScreen;
}

void WebView::trackSwipe(const QWheelEvent& event)
{
    switch (event.phase()) {
    case Qt::ScrollBegin:
        if (m_swipeFinish->isActive()) {
            m_swipeFinish->stop();
            finishSwipe(); // a new gesture before the last one was decided
        }
        if (swipeAllowed()) {
            m_swipe.begin();
        }
        break;
    case Qt::ScrollUpdate:
        m_swipe.update(event.pixelDelta());
        break;
    case Qt::ScrollEnd:
        if (m_swipe.active()) {
            m_swipe.end();
            m_swipeFinish->start(); // the page's last answers are still on their way
        }
        break;
    default: // no phase (a mouse wheel) or momentum: never a swipe
        break;
    }
}

void WebView::wheelEvent(QWheelEvent* event)
{
    if (m_swipe.active()) {
        m_swipe.unused(event->pixelDelta());
        showSwipeProgress();
        event->accept();
        return;
    }
    QWebEngineView::wheelEvent(event);
}

// RED_DEBUG_SWIPE=<seconds>:<dx>:<steps>[:<hold ms>][;…] (ADR-010): plays touchpad swipes into
// the page at those times after start, `steps` movements of `dx` px (classic scrolling:
// positive = towards the left edge = Back), holding before the fingers lift when asked (for a grab).
void WebView::scheduleDebugSwipes()
{
    const QString spec = qEnvironmentVariable("RED_DEBUG_SWIPE");
    for (const QString& one : spec.split(u';', Qt::SkipEmptyParts)) {
        const QStringList f = one.split(u':');
        const int at = f.value(0).toInt();
        const int dx = f.value(1).toInt();
        const int steps = f.value(2, u"12"_s).toInt();
        const int hold = f.value(3, u"0"_s).toInt();
        QTimer::singleShot(at * 1000, this, [this, dx, steps, hold] {
            QWidget* target = focusProxy() != nullptr ? focusProxy() : this;
            const QPointF pos(target->width() / 2.0, target->height() / 2.0);
            const QPointF global = target->mapToGlobal(pos);
            const auto send = [target, pos, global](Qt::ScrollPhase phase, QPoint delta) {
                QWheelEvent ev(pos, global, delta, delta * 2, Qt::NoButton, Qt::NoModifier, phase, false);
                QCoreApplication::sendEvent(target, &ev);
            };
            qCInfo(lcWeb) << "debug swipe" << dx << "x" << steps;
            send(Qt::ScrollBegin, {});
            for (int i = 1; i <= steps; ++i) {
                QTimer::singleShot(16 * i, target, [send, dx] { send(Qt::ScrollUpdate, QPoint(dx, 0)); });
            }
            QTimer::singleShot(16 * (steps + 1) + hold, target, [send] { send(Qt::ScrollEnd, {}); });
        });
    }
}

void WebView::showSwipeProgress()
{
    double progress = m_swipe.progress(width());
    if ((progress > 0 && !history()->canGoBack()) || (progress < 0 && !history()->canGoForward())) {
        progress = 0; // nowhere to go: no arrow, as in a browser
    }
    m_swipeIndicator->setProgress(progress);
}

void WebView::finishSwipe()
{
    const core::SwipeAction action = m_swipe.finish(width());
    m_swipeIndicator->setProgress(0);
    if (action == core::SwipeAction::Back && history()->canGoBack()) {
        qCInfo(lcWeb) << "swipe: back";
        back();
    } else if (action == core::SwipeAction::Forward && history()->canGoForward()) {
        qCInfo(lcWeb) << "swipe: forward";
        forward();
    }
}

void WebView::contextMenuEvent(QContextMenuEvent* event)
{
    const QWebEngineContextMenuRequest* request = lastContextMenuRequest();
    if (request == nullptr) {
        return;
    }
    if (m_profile->mode() == core::AppMode::Tv) {
        return; // right click is "back" in Leanback (tv/input.js)
    }
    const bool editable = request->isContentEditable();
    const bool hasSelection = !request->selectedText().isEmpty();
    const QUrl link = request->linkUrl();
    const QUrl downloadTarget = core::isDownloadable(link) ? link : downloadableUrl();

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (!downloadTarget.isEmpty()) {
        const core::YouTubeUrlKind kind = core::classifyYouTubeUrl(downloadTarget).kind;
        const QString label = kind == core::YouTubeUrlKind::Playlist  ? tr("Download playlist…")
                              : kind == core::YouTubeUrlKind::Channel ? tr("Download channel…")
                                                                      : tr("Download video…");
        QAction* download = menu->addAction(QIcon::fromTheme(u"download"_s), label);
        QFont bold = download->font();
        bold.setBold(true);
        download->setFont(bold);
        connect(download, &QAction::triggered, this,
                [this, downloadTarget] { Q_EMIT downloadRequested(downloadTarget); });
        // An action icon like the others (audio-x-generic is a coloured mime icon).
        QAction* audio =
            menu->addAction(QIcon::fromTheme(u"media-album-track"_s, QIcon::fromTheme(u"download"_s)),
                            tr("Download audio only…"));
        connect(audio, &QAction::triggered, this,
                [this, downloadTarget] { Q_EMIT audioDownloadRequested(downloadTarget); });
        menu->addSeparator();
    }
    if (!link.isEmpty()) {
        menu->addAction(pageAction(QWebEnginePage::CopyLinkToClipboard));
        QAction* external = menu->addAction(tr("Open link in browser"));
        connect(external, &QAction::triggered, this, [link] { platform::openUrl(link.toString()); });
        menu->addSeparator();
    }
    if (editable) {
        menu->addAction(pageAction(QWebEnginePage::Undo));
        menu->addAction(pageAction(QWebEnginePage::Redo));
        menu->addSeparator();
        menu->addAction(pageAction(QWebEnginePage::Cut));
    }
    if (hasSelection || editable) {
        menu->addAction(pageAction(QWebEnginePage::Copy));
    }
    if (editable) {
        menu->addAction(pageAction(QWebEnginePage::Paste));
        menu->addAction(pageAction(QWebEnginePage::SelectAll));
    }
    if (request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage) {
        menu->addAction(pageAction(QWebEnginePage::CopyImageToClipboard));
    }
    if (!editable && !hasSelection && link.isEmpty()) {
        menu->addAction(pageAction(QWebEnginePage::Back));
        menu->addAction(pageAction(QWebEnginePage::Forward));
        menu->addAction(pageAction(QWebEnginePage::Reload));
    }
    if (menu->isEmpty()) {
        delete menu;
        return;
    }
    menu->popup(event->globalPos());
}

} // namespace red::web
