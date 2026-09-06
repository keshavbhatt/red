#include "web/web_profile.h"

#include "core/blocking/block_list.h"
#include "core/theme/youtube_pref.h"
#include "web/bridge.h"
#include "web/cookie_exporter.h"
#include "web/logging.h"
#include "web/request_interceptor.h"
#include "web/script_bundle.h"
#include "web/user_agent.h"

#include <QDateTime>
#include <QDir>
#include <QNetworkCookie>
#include <QStandardPaths>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>

using namespace Qt::StringLiterals;

namespace red::web {

namespace {

constexpr auto kProfileName = "red";

QString themeName(core::Theme theme)
{
    switch (theme) {
    case core::Theme::Light:
        return u"light"_s;
    case core::Theme::Dark:
        return u"dark"_s;
    case core::Theme::System:
        break;
    }
    return u"system"_s;
}

QString codecName(core::CodecPreference codec)
{
    switch (codec) {
    case core::CodecPreference::H264:
        return u"h264"_s;
    case core::CodecPreference::Vp9:
        return u"vp9"_s;
    case core::CodecPreference::Av1:
        return u"av1"_s;
    case core::CodecPreference::Auto:
        break;
    }
    return u"auto"_s;
}

} // namespace

WebProfile::WebProfile(core::Settings& settings, const QString& appVersion, QObject* parent)
    : QWebEngineProfile(QString::fromLatin1(kProfileName), parent)
    , m_settings(settings)
    , m_appVersion(appVersion)
    , m_mode(settings.tvStartInTvMode() ? core::AppMode::Tv : settings.lastMode())
    , m_bridge(std::make_unique<Bridge>())
    , m_scripts(std::make_unique<ScriptBundle>(*this))
    , m_interceptor(std::make_unique<RequestInterceptor>())
    , m_cookies(std::make_unique<CookieExporter>(cookieStore()))
{
    configureStorage();
    configureUserAgent();
    configureAttributes();
    configureBlocking();
    installBootstrap();
    setUrlRequestInterceptor(m_interceptor.get());

    connect(&m_settings, &core::Settings::autoplayChanged, this, [this](bool on) {
        this->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, !on);
    });
    connect(&m_settings, &core::Settings::userAgentChanged, this, [this] { configureUserAgent(); });
    connect(&m_settings, &core::Settings::blockingChanged, this, [this] {
        configureBlocking();
        pushConfig();
    });
    // Anything that lands in the script config re-bakes the bootstrap (for the
    // next load) and pushes the live config (for the current page).
    for (auto signal : {&core::Settings::sponsorBlockChanged, &core::Settings::tvConfigChanged}) {
        connect(&m_settings, signal, this, [this] { pushConfig(); });
    }
    connect(&m_settings, &core::Settings::themeChanged, this, [this](core::Theme) {
        applyThemeCookie();
        pushConfig();
    });
    // YouTube keeps its theme in the PREF cookie; make it agree with ours as
    // soon as the store has loaded it (and whenever YouTube rewrites it). Not
    // at construction: the store is still empty then, and a bare PREF would
    // wipe YouTube's other flags (autoplay, playback quality…).
    connect(m_cookies.get(), &CookieExporter::cookieChanged, this, [this](const QNetworkCookie& c) {
        if (c.name() == "PREF" && c.domain().endsWith(u"youtube.com"_s)) {
            applyThemeCookie();
        }
    });
    connect(&m_settings, &core::Settings::hideShortsChanged, this, [this](bool) { pushConfig(); });
    connect(&m_settings, &core::Settings::codecPreferenceChanged, this,
            [this](core::CodecPreference) { pushConfig(); });
    connect(&m_settings, &core::Settings::playInBackgroundChanged, this, [this](bool) { pushConfig(); });
    qCInfo(lcWeb) << "profile ready, storage at" << persistentStoragePath();
}

WebProfile::~WebProfile()
{
    setUrlRequestInterceptor(nullptr);
}

void WebProfile::configureStorage()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString storage = base + u"/profile"_s;
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/profile"_s;
    QDir().mkpath(storage);
    QDir().mkpath(cache);

    setPersistentStoragePath(storage);
    setCachePath(cache);
    setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    setHttpCacheMaximumSize(512 * 1024 * 1024); // video sites churn the cache; cap it
    setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
}

QStringList WebProfile::storageRoots() const
{
    return {QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)};
}

void WebProfile::configureUserAgent()
{
    // The engine default is read once; effectiveUserAgent strips the QtWebEngine
    // token and applies the mode / override (ADR-001, ADR-008).
    static const QString engineDefault = QWebEngineProfile::defaultProfile()->httpUserAgent();
    setHttpUserAgent(effectiveUserAgent(engineDefault, m_settings.userAgentOverride(), m_mode,
                                        m_settings.signInUserAgent(), m_appVersion));
    if (m_mode == core::AppMode::Tv && m_settings.userAgentOverride().isEmpty()) {
        m_interceptor->setTvUserAgents(tvWireUserAgent(m_appVersion), tvGenericUserAgent(m_appVersion));
    } else {
        m_interceptor->setTvUserAgents({}, {});
    }
    qCDebug(lcWeb) << "user agent:" << httpUserAgent();
}

void WebProfile::configureAttributes()
{
    QWebEngineSettings* s = settings();
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    s->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    s->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, !m_settings.autoplay());
    s->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    s->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    s->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    s->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    s->setAttribute(QWebEngineSettings::ErrorPageEnabled, false); // we render our own (web/error_page)
    s->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    s->setAttribute(QWebEngineSettings::ShowScrollBars, m_mode == core::AppMode::Desktop);
    s->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
}

void WebProfile::configureBlocking()
{
    core::BlockList::Options options;
    options.ads = m_settings.blockAds();
    options.trackers = m_settings.blockTrackers();
    m_interceptor->setBlockList(std::make_shared<const core::BlockList>(core::BlockList::builtin(options)));
    qCInfo(lcWeb) << "blocking: ads" << options.ads << "trackers" << options.trackers;
}

QJsonObject WebProfile::scriptConfig() const
{
    QJsonObject sponsor{
        {u"enabled"_s, m_settings.sponsorBlockEnabled()},
        {u"markers"_s, m_settings.sponsorBlockMarkers()},
    };
    QJsonObject tv{
        {u"gamepad"_s, m_settings.tvGamepad()},
        {u"unlockResolution"_s, m_settings.tvUnlockResolution()},
        {u"lowMemory"_s, m_settings.tvLowMemory()},
        {u"hideShorts"_s, m_settings.tvHideShorts()},
    };
    return QJsonObject{
        {u"mode"_s, m_mode == core::AppMode::Tv ? u"tv"_s : u"desktop"_s},
        {u"appVersion"_s, m_appVersion},
        {u"colorScheme"_s, themeName(m_settings.theme())},
        {u"adblock"_s, m_settings.blockAds()},
        {u"hideShorts"_s, m_mode == core::AppMode::Tv ? m_settings.tvHideShorts() : m_settings.hideShorts()},
        {u"sponsorBlock"_s, sponsor},
        {u"codec"_s, codecName(m_settings.codecPreference())},
        {u"backgroundPlay"_s, m_settings.playInBackground()},
        {u"miniPlayer"_s, m_miniPlayer},
        {u"tv"_s, tv},
    };
}

void WebProfile::installBootstrap()
{
    m_scripts->installBootstrap(scriptConfig(), m_mode == core::AppMode::Tv);
}

void WebProfile::pushConfig()
{
    installBootstrap();
    Q_EMIT m_bridge->configChanged(scriptConfig());
}

void WebProfile::applyThemeCookie()
{
    const QString current = m_cookies->cookieValue(u"youtube.com"_s, "PREF");
    const QString wanted = core::youTubePrefForTheme(current, m_settings.theme());
    if (wanted == current || (wanted.isEmpty() && current.isEmpty())) {
        return;
    }
    if (wanted.isEmpty()) {
        return; // nothing but our bits would remain; YouTube regenerates the rest itself
    }
    QNetworkCookie cookie("PREF", wanted.toUtf8());
    cookie.setDomain(u".youtube.com"_s);
    cookie.setPath(u"/"_s);
    cookie.setSecure(true);
    cookie.setExpirationDate(QDateTime::currentDateTimeUtc().addYears(1));
    cookieStore()->setCookie(cookie, QUrl(u"https://www.youtube.com/"_s));
    qCDebug(lcWeb) << "youtube PREF theme bits set:" << wanted;
}

void WebProfile::setMode(core::AppMode mode)
{
    if (mode == m_mode) {
        return;
    }
    m_mode = mode;
    m_settings.setLastMode(mode);
    configureUserAgent();
    settings()->setAttribute(QWebEngineSettings::ShowScrollBars, m_mode == core::AppMode::Desktop);
    installBootstrap();
    qCInfo(lcWeb) << "mode:" << (mode == core::AppMode::Tv ? "tv" : "desktop");
}

void WebProfile::setMiniPlayer(bool on)
{
    if (m_miniPlayer == on) {
        return;
    }
    m_miniPlayer = on;
    installBootstrap();
    Q_EMIT m_bridge->miniPlayerChanged(on);
}

} // namespace red::web
