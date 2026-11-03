#include "core/settings/settings.h"

#include "core/logging.h"
#include "core/settings/settings_keys.h"
#include "core/zoom_policy.h"

#include <QDir>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

// ---- Defaults: the single source of truth ---------------------------------
constexpr CloseAction kDefaultCloseAction = CloseAction::Quit;
constexpr bool kDefaultShowRail = true;
constexpr bool kDefaultRailLabels = true; ///< new users see what each button is
constexpr AppMode kDefaultMode = AppMode::Desktop;
constexpr bool kDefaultDownloadsPanelOpen = false;
constexpr StartMode kDefaultStartMode = StartMode::Desktop;
constexpr bool kDefaultMiddleClickScroll = false;
constexpr bool kDefaultSwipeNavigation = true;
constexpr StartPage kDefaultStartPage = StartPage::Home;
constexpr bool kDefaultNotifyOnDownloadFinish = true;
constexpr bool kDefaultTrayEnabled = true;
constexpr Theme kDefaultTheme = Theme::System;
constexpr double kDefaultInterfaceScale = 1.0;
constexpr bool kDefaultAutoplay = true;
constexpr CodecPreference kDefaultCodec = CodecPreference::Auto;
constexpr bool kDefaultHardwareVideoDecode = false;
constexpr bool kDefaultKeepScreenAwake = true;
constexpr bool kDefaultTaskbarProgress = false;
constexpr MiniPlayerSize kDefaultMiniPlayerSize = MiniPlayerSize::Medium;
constexpr bool kDefaultPlayInBackground = true;
constexpr bool kDefaultBlockAds = true;
constexpr bool kDefaultBlockTrackers = true;
constexpr bool kDefaultHideShorts = false;
constexpr bool kDefaultShowDislikes = true;
constexpr bool kDefaultSponsorBlock = true;
constexpr bool kDefaultSponsorMarkers = true;
constexpr FilenamePattern kDefaultFilenamePattern = FilenamePattern::Title;
constexpr bool kDefaultOrganiseDownloads = true;
constexpr VideoQuality kDefaultQuality = VideoQuality::Best;
constexpr Container kDefaultContainer = Container::Mp4;
constexpr AudioFormat kDefaultAudioFormat = AudioFormat::Best;
constexpr DownloadKind kDefaultDownloadKind = DownloadKind::Video;
constexpr int kDefaultConcurrent = 2;
constexpr int kDefaultSpeedLimit = 0;
constexpr bool kDefaultUseSessionCookies = true;
constexpr bool kDefaultEmbedThumbnail = true;
constexpr bool kDefaultEmbedMetadata = true;
constexpr bool kDefaultRemoveSponsors = false;
constexpr bool kDefaultEngineAutoUpdate = true;
constexpr bool kDefaultEngineUseSystem = false;
constexpr bool kDefaultTvFullscreen = true;
constexpr bool kDefaultTvGamepad = true;
constexpr bool kDefaultTvHideShorts = false;
constexpr bool kDefaultTvUnlockResolution = false;
constexpr bool kDefaultTvLowMemory = false;
constexpr HardwareAcceleration kDefaultHardwareAcceleration = HardwareAcceleration::Auto;
constexpr SignInUserAgent kDefaultSignInUserAgent = SignInUserAgent::Firefox;

template <typename Enum>
Enum enumFromInt(int value, Enum def, int count)
{
    return (value >= 0 && value < count) ? static_cast<Enum>(value) : def;
}

} // namespace

QStringList sponsorCategories()
{
    return {u"sponsor"_s, u"selfpromo"_s, u"interaction"_s,    u"intro"_s,
            u"outro"_s,   u"preview"_s,   u"music_offtopic"_s, u"filler"_s};
}

SponsorAction defaultSponsorAction(const QString& category)
{
    if (category == u"sponsor"_s || category == u"selfpromo"_s || category == u"interaction"_s) {
        return SponsorAction::Skip;
    }
    if (category == u"intro"_s || category == u"outro"_s || category == u"preview"_s ||
        category == u"music_offtopic"_s) {
        return SponsorAction::Show;
    }
    return SponsorAction::Off;
}

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_store(std::make_unique<QSettings>())
{
    qCDebug(lcSettings) << "settings store:" << m_store->fileName();
    migrateGeneralGroup();
}

Settings::Settings(const QString& iniFilePath, QObject* parent)
    : QObject(parent)
    , m_store(std::make_unique<QSettings>(iniFilePath, QSettings::IniFormat))
{
    qCDebug(lcSettings) << "settings store (ini):" << m_store->fileName();
    migrateGeneralGroup();
}

/// Up to 10.2 Settings, General lived under "general/", which QSettings'
/// INI format writes as [%General] and reads back as "General/": the values
/// were on disk but never found. Move them to "app/" once, keeping the user's
/// choices (a value already under "app/" wins).
void Settings::migrateGeneralGroup()
{
    for (const QLatin1StringView key : {keys::kStartMode, keys::kStartPage, keys::kMiddleClickScroll,
                                        keys::kNotifyOnDownloadFinish, keys::kTrayEnabled}) {
        const QString name = QString(key).section(u'/', 1);
        for (const QString& old : QStringList{u"General/"_s + name, u"general/"_s + name}) {
            if (m_store->contains(old)) {
                if (!m_store->contains(key)) {
                    m_store->setValue(key, m_store->value(old));
                }
                m_store->remove(old);
            }
        }
    }
}

Settings::~Settings()
{
    m_store->sync();
}

// ---- helpers ---------------------------------------------------------------

bool Settings::boolValue(QLatin1StringView key, bool def) const
{
    return m_store->value(key, def).toBool();
}

bool Settings::storeBool(QLatin1StringView key, bool def, bool value)
{
    if (boolValue(key, def) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

int Settings::intValue(QLatin1StringView key, int def) const
{
    return m_store->value(key, def).toInt();
}

bool Settings::storeInt(QLatin1StringView key, int def, int value)
{
    if (intValue(key, def) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

QString Settings::stringValue(QLatin1StringView key, const QString& def) const
{
    return m_store->value(key, def).toString();
}

bool Settings::storeString(QLatin1StringView key, const QString& value)
{
    if (stringValue(key) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

// ---- window/ ---------------------------------------------------------------

QByteArray Settings::windowGeometry() const
{
    return m_store->value(keys::kWindowGeometry).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    m_store->setValue(keys::kWindowGeometry, geometry);
}

QByteArray Settings::windowState() const
{
    return m_store->value(keys::kWindowState).toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    m_store->setValue(keys::kWindowState, state);
}

QByteArray Settings::settingsDialogGeometry() const
{
    return m_store->value(keys::kSettingsDialogGeometry).toByteArray();
}

void Settings::setSettingsDialogGeometry(const QByteArray& geometry)
{
    m_store->setValue(keys::kSettingsDialogGeometry, geometry);
}

CloseAction Settings::closeAction() const
{
    return enumFromInt(intValue(keys::kCloseAction, static_cast<int>(kDefaultCloseAction)),
                       kDefaultCloseAction, 2);
}

void Settings::setCloseAction(CloseAction action)
{
    if (storeInt(keys::kCloseAction, static_cast<int>(kDefaultCloseAction), static_cast<int>(action))) {
        Q_EMIT closeActionChanged(action);
    }
}

bool Settings::showRail() const
{
    return boolValue(keys::kShowRail, kDefaultShowRail);
}

void Settings::setShowRail(bool show)
{
    if (storeBool(keys::kShowRail, kDefaultShowRail, show)) {
        Q_EMIT showRailChanged(show);
    }
}

bool Settings::railLabels() const
{
    return boolValue(keys::kRailLabels, kDefaultRailLabels);
}

void Settings::setRailLabels(bool show)
{
    if (storeBool(keys::kRailLabels, kDefaultRailLabels, show)) {
        Q_EMIT railLabelsChanged(show);
    }
}

AppMode Settings::lastMode() const
{
    return enumFromInt(intValue(keys::kLastMode, static_cast<int>(kDefaultMode)), kDefaultMode, 3);
}

void Settings::setLastMode(AppMode mode)
{
    storeInt(keys::kLastMode, static_cast<int>(kDefaultMode), static_cast<int>(mode));
}

QString Settings::lastUrl() const
{
    return stringValue(keys::kLastUrl);
}

void Settings::setLastUrl(const QString& url)
{
    storeString(keys::kLastUrl, url);
}

QString Settings::lastMusicUrl() const
{
    return stringValue(keys::kLastMusicUrl);
}

void Settings::setLastMusicUrl(const QString& url)
{
    storeString(keys::kLastMusicUrl, url);
}

bool Settings::downloadsPanelOpen() const
{
    return boolValue(keys::kDownloadsPanelOpen, kDefaultDownloadsPanelOpen);
}

void Settings::setDownloadsPanelOpen(bool open)
{
    storeBool(keys::kDownloadsPanelOpen, kDefaultDownloadsPanelOpen, open);
}

// ---- general/ --------------------------------------------------------------

StartMode Settings::startMode() const
{
    if (!m_store->contains(keys::kStartMode)) {
        // "Start Red in TV mode" was the only start choice before; honour it.
        return boolValue(keys::kLegacyTvStartInTvMode, false) ? StartMode::Tv : kDefaultStartMode;
    }
    return enumFromInt(intValue(keys::kStartMode, static_cast<int>(kDefaultStartMode)), kDefaultStartMode, 4);
}

void Settings::setStartMode(StartMode mode)
{
    const StartMode before = startMode();
    m_store->remove(keys::kLegacyTvStartInTvMode);
    m_store->setValue(keys::kStartMode, static_cast<int>(mode));
    if (before != mode) {
        Q_EMIT startModeChanged(mode);
    }
}

AppMode Settings::startupMode() const
{
    switch (startMode()) {
    case StartMode::Music:
        return AppMode::Music;
    case StartMode::Tv:
        return AppMode::Tv;
    case StartMode::LastUsed:
        return lastMode();
    case StartMode::Desktop:
        break;
    }
    return AppMode::Desktop;
}

bool Settings::middleClickScroll() const
{
    return boolValue(keys::kMiddleClickScroll, kDefaultMiddleClickScroll);
}

void Settings::setMiddleClickScroll(bool enabled)
{
    if (storeBool(keys::kMiddleClickScroll, kDefaultMiddleClickScroll, enabled)) {
        Q_EMIT middleClickScrollChanged(enabled);
    }
}

bool Settings::swipeNavigation() const
{
    return boolValue(keys::kSwipeNavigation, kDefaultSwipeNavigation);
}

void Settings::setSwipeNavigation(bool enabled)
{
    if (storeBool(keys::kSwipeNavigation, kDefaultSwipeNavigation, enabled)) {
        Q_EMIT swipeNavigationChanged(enabled);
    }
}

StartPage Settings::startPage() const
{
    return enumFromInt(intValue(keys::kStartPage, static_cast<int>(kDefaultStartPage)), kDefaultStartPage, 3);
}

void Settings::setStartPage(StartPage page)
{
    if (storeInt(keys::kStartPage, static_cast<int>(kDefaultStartPage), static_cast<int>(page))) {
        Q_EMIT startPageChanged(page);
    }
}

bool Settings::notifyOnDownloadFinish() const
{
    return boolValue(keys::kNotifyOnDownloadFinish, kDefaultNotifyOnDownloadFinish);
}

void Settings::setNotifyOnDownloadFinish(bool enabled)
{
    if (storeBool(keys::kNotifyOnDownloadFinish, kDefaultNotifyOnDownloadFinish, enabled)) {
        Q_EMIT notifyOnDownloadFinishChanged(enabled);
    }
}

bool Settings::trayEnabled() const
{
    return boolValue(keys::kTrayEnabled, kDefaultTrayEnabled);
}

void Settings::setTrayEnabled(bool enabled)
{
    if (storeBool(keys::kTrayEnabled, kDefaultTrayEnabled, enabled)) {
        Q_EMIT trayEnabledChanged(enabled);
    }
}

// ---- appearance/ -----------------------------------------------------------

Theme Settings::theme() const
{
    return enumFromInt(intValue(keys::kTheme, static_cast<int>(kDefaultTheme)), kDefaultTheme, 3);
}

void Settings::setTheme(Theme theme)
{
    if (storeInt(keys::kTheme, static_cast<int>(kDefaultTheme), static_cast<int>(theme))) {
        Q_EMIT themeChanged(theme);
    }
}

double Settings::zoomFactor() const
{
    return clampZoom(m_store->value(keys::kZoomFactor, kDefaultZoom).toDouble());
}

void Settings::setZoomFactor(double factor)
{
    const double clamped = clampZoom(factor);
    if (qFuzzyCompare(zoomFactor(), clamped)) {
        return;
    }
    m_store->setValue(keys::kZoomFactor, clamped);
    Q_EMIT zoomFactorChanged(clamped);
}

double Settings::interfaceScale() const
{
    return std::clamp(m_store->value(keys::kInterfaceScale, kDefaultInterfaceScale).toDouble(),
                      kMinInterfaceScale, kMaxInterfaceScale);
}

void Settings::setInterfaceScale(double scale)
{
    const double clamped = std::clamp(scale, kMinInterfaceScale, kMaxInterfaceScale);
    if (qFuzzyCompare(interfaceScale(), clamped)) {
        return;
    }
    m_store->setValue(keys::kInterfaceScale, clamped);
    Q_EMIT interfaceScaleChanged(clamped);
}

// ---- playback/ -------------------------------------------------------------

bool Settings::autoplay() const
{
    return boolValue(keys::kAutoplay, kDefaultAutoplay);
}

void Settings::setAutoplay(bool enabled)
{
    if (storeBool(keys::kAutoplay, kDefaultAutoplay, enabled)) {
        Q_EMIT autoplayChanged(enabled);
    }
}

CodecPreference Settings::codecPreference() const
{
    return enumFromInt(intValue(keys::kCodecPreference, static_cast<int>(kDefaultCodec)), kDefaultCodec, 4);
}

void Settings::setCodecPreference(CodecPreference codec)
{
    if (storeInt(keys::kCodecPreference, static_cast<int>(kDefaultCodec), static_cast<int>(codec))) {
        Q_EMIT codecPreferenceChanged(codec);
    }
}

bool Settings::hardwareVideoDecode() const
{
    return boolValue(keys::kHardwareVideoDecode, kDefaultHardwareVideoDecode);
}

void Settings::setHardwareVideoDecode(bool enabled)
{
    if (storeBool(keys::kHardwareVideoDecode, kDefaultHardwareVideoDecode, enabled)) {
        Q_EMIT hardwareVideoDecodeChanged(enabled);
    }
}

bool Settings::keepScreenAwake() const
{
    return boolValue(keys::kKeepScreenAwake, kDefaultKeepScreenAwake);
}

void Settings::setKeepScreenAwake(bool enabled)
{
    if (storeBool(keys::kKeepScreenAwake, kDefaultKeepScreenAwake, enabled)) {
        Q_EMIT keepScreenAwakeChanged(enabled);
    }
}

bool Settings::taskbarProgress() const
{
    return boolValue(keys::kTaskbarProgress, kDefaultTaskbarProgress);
}

void Settings::setTaskbarProgress(bool enabled)
{
    if (storeBool(keys::kTaskbarProgress, kDefaultTaskbarProgress, enabled)) {
        Q_EMIT taskbarProgressChanged(enabled);
    }
}

MiniPlayerSize Settings::miniPlayerSize() const
{
    return enumFromInt(intValue(keys::kMiniPlayerSize, static_cast<int>(kDefaultMiniPlayerSize)),
                       kDefaultMiniPlayerSize, 3);
}

void Settings::setMiniPlayerSize(MiniPlayerSize size)
{
    if (storeInt(keys::kMiniPlayerSize, static_cast<int>(kDefaultMiniPlayerSize), static_cast<int>(size))) {
        Q_EMIT miniPlayerSizeChanged(size);
    }
}

bool Settings::playInBackground() const
{
    return boolValue(keys::kPlayInBackground, kDefaultPlayInBackground);
}

void Settings::setPlayInBackground(bool enabled)
{
    if (storeBool(keys::kPlayInBackground, kDefaultPlayInBackground, enabled)) {
        Q_EMIT playInBackgroundChanged(enabled);
    }
}

// ---- blocking/ -------------------------------------------------------------

bool Settings::blockAds() const
{
    return boolValue(keys::kBlockAds, kDefaultBlockAds);
}

void Settings::setBlockAds(bool enabled)
{
    if (storeBool(keys::kBlockAds, kDefaultBlockAds, enabled)) {
        Q_EMIT blockingChanged();
    }
}

bool Settings::blockTrackers() const
{
    return boolValue(keys::kBlockTrackers, kDefaultBlockTrackers);
}

void Settings::setBlockTrackers(bool enabled)
{
    if (storeBool(keys::kBlockTrackers, kDefaultBlockTrackers, enabled)) {
        Q_EMIT blockingChanged();
    }
}

bool Settings::hideShorts() const
{
    return boolValue(keys::kHideShorts, kDefaultHideShorts);
}

void Settings::setHideShorts(bool enabled)
{
    if (storeBool(keys::kHideShorts, kDefaultHideShorts, enabled)) {
        Q_EMIT hideShortsChanged(enabled);
    }
}

bool Settings::showDislikes() const
{
    return boolValue(keys::kShowDislikes, kDefaultShowDislikes);
}

void Settings::setShowDislikes(bool enabled)
{
    if (storeBool(keys::kShowDislikes, kDefaultShowDislikes, enabled)) {
        Q_EMIT showDislikesChanged(enabled);
    }
}

bool Settings::sponsorBlockEnabled() const
{
    return boolValue(keys::kSponsorBlockEnabled, kDefaultSponsorBlock);
}

void Settings::setSponsorBlockEnabled(bool enabled)
{
    if (storeBool(keys::kSponsorBlockEnabled, kDefaultSponsorBlock, enabled)) {
        Q_EMIT sponsorBlockChanged();
    }
}

SponsorAction Settings::sponsorAction(const QString& category) const
{
    const QVariantMap map = m_store->value(keys::kSponsorBlockCategories).toMap();
    const SponsorAction def = defaultSponsorAction(category);
    if (!map.contains(category)) {
        return def;
    }
    return enumFromInt(map.value(category).toInt(), def, 3);
}

void Settings::setSponsorAction(const QString& category, SponsorAction action)
{
    if (sponsorAction(category) == action) {
        return;
    }
    QVariantMap map = m_store->value(keys::kSponsorBlockCategories).toMap();
    map.insert(category, static_cast<int>(action));
    m_store->setValue(keys::kSponsorBlockCategories, map);
    Q_EMIT sponsorBlockChanged();
}

QHash<QString, SponsorAction> Settings::sponsorActions() const
{
    QHash<QString, SponsorAction> result;
    for (const QString& category : sponsorCategories()) {
        result.insert(category, sponsorAction(category));
    }
    return result;
}

bool Settings::sponsorBlockMarkers() const
{
    return boolValue(keys::kSponsorBlockMarkers, kDefaultSponsorMarkers);
}

void Settings::setSponsorBlockMarkers(bool enabled)
{
    if (storeBool(keys::kSponsorBlockMarkers, kDefaultSponsorMarkers, enabled)) {
        Q_EMIT sponsorBlockChanged();
    }
}

quint64 Settings::blockedCount() const
{
    return m_store->value(keys::kBlockedCount, 0).toULongLong();
}

void Settings::setBlockedCount(quint64 count)
{
    m_store->setValue(keys::kBlockedCount, count);
}

// ---- downloads/ ------------------------------------------------------------

QString Settings::downloadDirectory() const
{
    const QString stored = stringValue(keys::kDownloadDirectory);
    if (!stored.isEmpty() && QDir(stored).isAbsolute()) {
        return stored;
    }
    // ~/Downloads/Red: the folder every desktop shows for downloads.
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(u"Red"_s);
}

void Settings::setDownloadDirectory(const QString& directory)
{
    if (storeString(keys::kDownloadDirectory, directory)) {
        Q_EMIT downloadDirectoryChanged(downloadDirectory());
    }
}

FilenamePattern Settings::filenamePattern() const
{
    return enumFromInt(intValue(keys::kFilenamePattern, static_cast<int>(kDefaultFilenamePattern)),
                       kDefaultFilenamePattern, 3);
}

void Settings::setFilenamePattern(FilenamePattern pattern)
{
    if (storeInt(keys::kFilenamePattern, static_cast<int>(kDefaultFilenamePattern),
                 static_cast<int>(pattern))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::organiseDownloads() const
{
    return boolValue(keys::kOrganiseDownloads, kDefaultOrganiseDownloads);
}

void Settings::setOrganiseDownloads(bool enabled)
{
    if (storeBool(keys::kOrganiseDownloads, kDefaultOrganiseDownloads, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

VideoQuality Settings::defaultQuality() const
{
    return enumFromInt(intValue(keys::kDefaultQuality, static_cast<int>(kDefaultQuality)), kDefaultQuality,
                       7);
}

void Settings::setDefaultQuality(VideoQuality quality)
{
    if (storeInt(keys::kDefaultQuality, static_cast<int>(kDefaultQuality), static_cast<int>(quality))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

Container Settings::defaultContainer() const
{
    return enumFromInt(intValue(keys::kDefaultContainer, static_cast<int>(kDefaultContainer)),
                       kDefaultContainer, 3);
}

void Settings::setDefaultContainer(Container container)
{
    if (storeInt(keys::kDefaultContainer, static_cast<int>(kDefaultContainer), static_cast<int>(container))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

AudioFormat Settings::defaultAudioFormat() const
{
    return enumFromInt(intValue(keys::kDefaultAudioFormat, static_cast<int>(kDefaultAudioFormat)),
                       kDefaultAudioFormat, 6);
}

void Settings::setDefaultAudioFormat(AudioFormat format)
{
    if (storeInt(keys::kDefaultAudioFormat, static_cast<int>(kDefaultAudioFormat),
                 static_cast<int>(format))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

DownloadKind Settings::lastDownloadKind() const
{
    return enumFromInt(intValue(keys::kLastDownloadKind, static_cast<int>(kDefaultDownloadKind)),
                       kDefaultDownloadKind, 3);
}

void Settings::setLastDownloadKind(DownloadKind kind)
{
    // Not a "default" the dialog re-reads live: it is only consulted on open.
    storeInt(keys::kLastDownloadKind, static_cast<int>(kDefaultDownloadKind), static_cast<int>(kind));
}

int Settings::concurrentDownloads() const
{
    return std::clamp(intValue(keys::kConcurrentDownloads, kDefaultConcurrent), 1, kMaxConcurrentDownloads);
}

void Settings::setConcurrentDownloads(int count)
{
    const int clamped = std::clamp(count, 1, kMaxConcurrentDownloads);
    if (storeInt(keys::kConcurrentDownloads, kDefaultConcurrent, clamped)) {
        Q_EMIT concurrentDownloadsChanged(clamped);
    }
}

int Settings::speedLimitKbps() const
{
    return std::max(0, intValue(keys::kSpeedLimitKbps, kDefaultSpeedLimit));
}

void Settings::setSpeedLimitKbps(int kbps)
{
    const int clamped = std::max(0, kbps);
    if (storeInt(keys::kSpeedLimitKbps, kDefaultSpeedLimit, clamped)) {
        Q_EMIT speedLimitChanged(clamped);
    }
}

bool Settings::useSessionCookies() const
{
    return boolValue(keys::kUseSessionCookies, kDefaultUseSessionCookies);
}

void Settings::setUseSessionCookies(bool enabled)
{
    if (storeBool(keys::kUseSessionCookies, kDefaultUseSessionCookies, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::embedThumbnail() const
{
    return boolValue(keys::kEmbedThumbnail, kDefaultEmbedThumbnail);
}

void Settings::setEmbedThumbnail(bool enabled)
{
    if (storeBool(keys::kEmbedThumbnail, kDefaultEmbedThumbnail, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::embedMetadata() const
{
    return boolValue(keys::kEmbedMetadata, kDefaultEmbedMetadata);
}

void Settings::setEmbedMetadata(bool enabled)
{
    if (storeBool(keys::kEmbedMetadata, kDefaultEmbedMetadata, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::removeSponsors() const
{
    return boolValue(keys::kRemoveSponsors, kDefaultRemoveSponsors);
}

void Settings::setRemoveSponsors(bool enabled)
{
    if (storeBool(keys::kRemoveSponsors, kDefaultRemoveSponsors, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

QStringList Settings::subtitleLanguages() const
{
    return m_store->value(keys::kSubtitleLanguages).toStringList();
}

void Settings::setSubtitleLanguages(const QStringList& languages)
{
    if (subtitleLanguages() == languages) {
        return;
    }
    m_store->setValue(keys::kSubtitleLanguages, languages);
    Q_EMIT downloadDefaultsChanged();
}

// ---- engine/ ---------------------------------------------------------------

bool Settings::engineAutoUpdate() const
{
    return boolValue(keys::kEngineAutoUpdate, kDefaultEngineAutoUpdate);
}

void Settings::setEngineAutoUpdate(bool enabled)
{
    if (storeBool(keys::kEngineAutoUpdate, kDefaultEngineAutoUpdate, enabled)) {
        Q_EMIT engineConfigChanged();
    }
}

bool Settings::engineUseSystem() const
{
    return boolValue(keys::kEngineUseSystem, kDefaultEngineUseSystem);
}

void Settings::setEngineUseSystem(bool enabled)
{
    if (storeBool(keys::kEngineUseSystem, kDefaultEngineUseSystem, enabled)) {
        Q_EMIT engineConfigChanged();
    }
}

QString Settings::engineSystemPath() const
{
    return stringValue(keys::kEngineSystemPath);
}

void Settings::setEngineSystemPath(const QString& path)
{
    if (storeString(keys::kEngineSystemPath, path)) {
        Q_EMIT engineConfigChanged();
    }
}

QDateTime Settings::engineLastCheck() const
{
    return m_store->value(keys::kEngineLastCheck).toDateTime();
}

void Settings::setEngineLastCheck(const QDateTime& when)
{
    m_store->setValue(keys::kEngineLastCheck, when);
}

// ---- tv/ -------------------------------------------------------------------

bool Settings::tvFullscreen() const
{
    return boolValue(keys::kTvFullscreen, kDefaultTvFullscreen);
}

void Settings::setTvFullscreen(bool enabled)
{
    if (storeBool(keys::kTvFullscreen, kDefaultTvFullscreen, enabled)) {
        Q_EMIT tvConfigChanged();
    }
}

bool Settings::tvGamepad() const
{
    return boolValue(keys::kTvGamepad, kDefaultTvGamepad);
}

void Settings::setTvGamepad(bool enabled)
{
    if (storeBool(keys::kTvGamepad, kDefaultTvGamepad, enabled)) {
        Q_EMIT tvConfigChanged();
    }
}

bool Settings::tvHideShorts() const
{
    return boolValue(keys::kTvHideShorts, kDefaultTvHideShorts);
}

void Settings::setTvHideShorts(bool enabled)
{
    if (storeBool(keys::kTvHideShorts, kDefaultTvHideShorts, enabled)) {
        Q_EMIT tvConfigChanged();
    }
}

bool Settings::tvUnlockResolution() const
{
    return boolValue(keys::kTvUnlockResolution, kDefaultTvUnlockResolution);
}

void Settings::setTvUnlockResolution(bool enabled)
{
    if (storeBool(keys::kTvUnlockResolution, kDefaultTvUnlockResolution, enabled)) {
        Q_EMIT tvConfigChanged();
    }
}

bool Settings::tvLowMemory() const
{
    return boolValue(keys::kTvLowMemory, kDefaultTvLowMemory);
}

void Settings::setTvLowMemory(bool enabled)
{
    if (storeBool(keys::kTvLowMemory, kDefaultTvLowMemory, enabled)) {
        Q_EMIT tvConfigChanged();
    }
}

// ---- advanced/ -------------------------------------------------------------

HardwareAcceleration Settings::hardwareAcceleration() const
{
    return enumFromInt(intValue(keys::kHardwareAcceleration, static_cast<int>(kDefaultHardwareAcceleration)),
                       kDefaultHardwareAcceleration, 3);
}

void Settings::setHardwareAcceleration(HardwareAcceleration mode)
{
    if (storeInt(keys::kHardwareAcceleration, static_cast<int>(kDefaultHardwareAcceleration),
                 static_cast<int>(mode))) {
        // An explicit choice restarts the GPU trial (ADR-032 lineage).
        setGpuAutoDisabled(false);
        setGpuProbeStrikes(0);
        Q_EMIT hardwareAccelerationChanged(mode);
    }
}

bool Settings::gpuAutoDisabled() const
{
    return boolValue(keys::kGpuAutoDisabled, false);
}

void Settings::setGpuAutoDisabled(bool disabled)
{
    if (storeBool(keys::kGpuAutoDisabled, false, disabled)) {
        Q_EMIT gpuAutoDisabledChanged(disabled);
    }
}

int Settings::gpuProbeStrikes() const
{
    return std::max(0, intValue(keys::kGpuProbeStrikes, 0));
}

void Settings::setGpuProbeStrikes(int strikes)
{
    storeInt(keys::kGpuProbeStrikes, 0, std::max(0, strikes));
}

bool Settings::gpuFallbackNotice() const
{
    return boolValue(keys::kGpuFallbackNotice, false);
}

void Settings::setGpuFallbackNotice(bool pending)
{
    storeBool(keys::kGpuFallbackNotice, false, pending);
}

bool Settings::videoDecodeFallbackNotice() const
{
    return boolValue(keys::kVideoDecodeFallbackNotice, false);
}

void Settings::setVideoDecodeFallbackNotice(bool pending)
{
    storeBool(keys::kVideoDecodeFallbackNotice, false, pending);
}

bool Settings::welcomeSeen() const
{
    return boolValue(keys::kWelcomeSeen, false);
}

void Settings::setWelcomeSeen(bool seen)
{
    storeBool(keys::kWelcomeSeen, false, seen);
}

bool Settings::isFirstRun() const
{
    return !welcomeSeen() && whatsNewSeenVersion().isEmpty();
}

QString Settings::whatsNewSeenVersion() const
{
    return stringValue(keys::kWhatsNewSeenVersion).trimmed();
}

void Settings::setWhatsNewSeenVersion(const QString& version)
{
    storeString(keys::kWhatsNewSeenVersion, version.trimmed());
}

DailyAllowance Settings::freeDownloads() const
{
    return {stringValue(keys::kFreeDownloadsDay), intValue(keys::kFreeDownloadsUsed, 0)};
}

void Settings::setFreeDownloads(const DailyAllowance& allowance)
{
    storeString(keys::kFreeDownloadsDay, allowance.day);
    storeInt(keys::kFreeDownloadsUsed, 0, allowance.used);
}

QString Settings::userAgentOverride() const
{
    return stringValue(keys::kUserAgentOverride).trimmed();
}

void Settings::setUserAgentOverride(const QString& userAgent)
{
    if (storeString(keys::kUserAgentOverride, userAgent.trimmed())) {
        Q_EMIT userAgentChanged();
    }
}

SignInUserAgent Settings::signInUserAgent() const
{
    return enumFromInt(intValue(keys::kSignInUserAgent, static_cast<int>(kDefaultSignInUserAgent)),
                       kDefaultSignInUserAgent, 2);
}

void Settings::setSignInUserAgent(SignInUserAgent agent)
{
    if (storeInt(keys::kSignInUserAgent, static_cast<int>(kDefaultSignInUserAgent),
                 static_cast<int>(agent))) {
        Q_EMIT userAgentChanged();
    }
}

// ---- misc ------------------------------------------------------------------

void Settings::sync()
{
    m_store->sync();
}

void Settings::resetToDefaults()
{
    const DailyAllowance downloads = freeDownloads(); // not a preference: survives the reset
    const QStringList allKeys = m_store->allKeys();
    for (const QString& key : allKeys) {
        m_store->remove(key);
    }
    setFreeDownloads(downloads);
    m_store->sync();
    Q_EMIT closeActionChanged(closeAction());
    Q_EMIT showRailChanged(showRail());
    Q_EMIT railLabelsChanged(railLabels());
    Q_EMIT startModeChanged(startMode());
    Q_EMIT startPageChanged(startPage());
    Q_EMIT middleClickScrollChanged(middleClickScroll());
    Q_EMIT swipeNavigationChanged(swipeNavigation());
    Q_EMIT notifyOnDownloadFinishChanged(notifyOnDownloadFinish());
    Q_EMIT trayEnabledChanged(trayEnabled());
    Q_EMIT themeChanged(theme());
    Q_EMIT zoomFactorChanged(zoomFactor());
    Q_EMIT autoplayChanged(autoplay());
    Q_EMIT codecPreferenceChanged(codecPreference());
    Q_EMIT keepScreenAwakeChanged(keepScreenAwake());
    Q_EMIT taskbarProgressChanged(taskbarProgress());
    Q_EMIT miniPlayerSizeChanged(miniPlayerSize());
    Q_EMIT playInBackgroundChanged(playInBackground());
    Q_EMIT blockingChanged();
    Q_EMIT hideShortsChanged(hideShorts());
    Q_EMIT showDislikesChanged(showDislikes());
    Q_EMIT sponsorBlockChanged();
    Q_EMIT downloadDirectoryChanged(downloadDirectory());
    Q_EMIT downloadDefaultsChanged();
    Q_EMIT concurrentDownloadsChanged(concurrentDownloads());
    Q_EMIT speedLimitChanged(speedLimitKbps());
    Q_EMIT engineConfigChanged();
    Q_EMIT tvConfigChanged();
    Q_EMIT hardwareAccelerationChanged(hardwareAcceleration());
    Q_EMIT userAgentChanged();
}

QString Settings::fileName() const
{
    return m_store->fileName();
}

} // namespace red::core
