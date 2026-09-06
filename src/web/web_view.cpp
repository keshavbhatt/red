#include "web/web_view.h"

#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "core/zoom_policy.h"
#include "platform/file_manager.h"
#include "web/bridge.h"
#include "web/error_page.h"
#include "web/logging.h"
#include "web/permission_controller.h"
#include "web/popup_window.h"
#include "web/web_page.h"
#include "web/web_profile.h"

#include <QAction>
#include <QChildEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <QWebChannel>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineLoadingInfo>
#include <QWebEngineScript>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

namespace red::web {

namespace {
const QUrl kHomeUrl(u"https://www.youtube.com/"_s);
const QUrl kSubscriptionsUrl(u"https://www.youtube.com/feed/subscriptions"_s);
const QUrl kTvUrl(u"https://www.youtube.com/tv"_s);
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

    // One channel per page exposing the profile's bridge object (ADR-006).
    auto* channel = new QWebChannel(m_page);
    channel->registerObject(u"bridge"_s, &m_profile->bridge());
    m_page->setWebChannel(channel, QWebEngineScript::MainWorld);

    m_permissions->attach(*m_page);
    connect(m_permissions, &PermissionController::promptRequested, this, &WebView::permissionPromptRequested);
    connect(m_page, &WebPage::popupOpened, this, &WebView::wirePopup);

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
            if (m_profile->mode() == core::AppMode::Desktop && !m_showingError) {
                m_settings.setLastUrl(url().toString());
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
    switch (m_settings.startPage()) {
    case core::StartPage::Subscriptions:
        return kSubscriptionsUrl;
    case core::StartPage::LastVisited: {
        const QUrl last(m_settings.lastUrl());
        if (last.isValid() && core::isYouTubeHost(last.host())) {
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

void WebView::setMode(core::AppMode mode)
{
    if (mode == m_profile->mode()) {
        return;
    }
    // Carry the video across: a watch page becomes the Leanback route and back.
    const std::optional<QUrl> current = core::canonicalVideoUrl(url());
    const QString videoId = current ? core::classifyYouTubeUrl(*current).videoId : m_media.videoId;
    m_profile->setMode(mode);
    if (!videoId.isEmpty()) {
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
    load(url);
}

void WebView::goHome()
{
    load(m_profile->mode() == core::AppMode::Tv ? kTvUrl : kHomeUrl);
}

void WebView::handleUrlChanged(const QUrl& url)
{
    const bool downloadable = core::isDownloadable(url);
    if (downloadable != m_downloadable) {
        m_downloadable = downloadable;
        Q_EMIT downloadableChanged(downloadable);
    }
}

QUrl WebView::downloadableUrl() const
{
    if (m_profile->mode() == core::AppMode::Tv) {
        const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(url());
        const QString id = !info.videoId.isEmpty() ? info.videoId : m_media.videoId;
        return id.isEmpty() ? QUrl() : QUrl(u"https://www.youtube.com/watch?v="_s + id);
    }
    if (core::isDownloadable(url())) {
        return url();
    }
    if (!m_media.videoId.isEmpty()) {
        return QUrl(u"https://www.youtube.com/watch?v="_s + m_media.videoId);
    }
    return {};
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
}

void WebView::mediaCommand(const QString& command, double argument)
{
    Q_EMIT m_profile->bridge().mediaCommand(command, argument);
}

void WebView::setMiniPlayer(bool on)
{
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
    }
    return QWebEngineView::eventFilter(watched, event);
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
    const QUrl downloadTarget =
        core::isDownloadable(link) ? link : (core::isDownloadable(url()) ? url() : QUrl());

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
