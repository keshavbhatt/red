#pragma once

#include "core/licensing/daily_allowance.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QLatin1StringView>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace red::core {

enum class Theme
{
    System,
    Light,
    Dark,
};

enum class CloseAction
{
    Quit,
    MinimizeToTray,
};

enum class StartPage
{
    Home,
    Subscriptions,
    LastVisited,
};

/// Which mode a new launch opens in (Settings > General). A mode asked for on
/// the command line wins over it.
enum class StartMode
{
    Desktop,
    Music,
    Tv,
    LastUsed, ///< the mode Red was in when it last closed
};

enum class AppMode
{
    Desktop,
    Tv,
    Music, ///< music.youtube.com in the same window
};

/// The mode's name as used in logs, diagnostics and the page script config.
[[nodiscard]] constexpr QLatin1StringView appModeName(AppMode mode) noexcept
{
    switch (mode) {
    case AppMode::Tv:
        return QLatin1StringView("tv");
    case AppMode::Music:
        return QLatin1StringView("music");
    case AppMode::Desktop:
        break;
    }
    return QLatin1StringView("desktop");
}

enum class CodecPreference
{
    Auto, ///< YouTube decides (usually VP9/AV1)
    H264, ///< lowest CPU, widest hardware support
    Vp9,
    Av1,
};

enum class MiniPlayerSize
{
    Small,  ///< 400×225
    Medium, ///< 480×270
    Large,  ///< 640×360
};

enum class HardwareAcceleration
{
    Auto, ///< Chromium decides (GPU on unless blocklisted)
    On,   ///< ignore the GPU blocklist
    Off,  ///< --disable-gpu
};

/// What Google's sign-in pages are told about the browser (ADR-008). Only
/// the User-Agent header on accounts.google.com / accounts.youtube.com is
/// affected; YouTube itself always sees the sanitized Chrome UA.
enum class SignInUserAgent
{
    Firefox, ///< default: Google accepts Firefox where it rejects embedded Chrome
    Chrome,  ///< same as the rest of the app
};

/// Quality ceiling for the video preset; the selector is bv*[height<=N]+ba.
enum class VideoQuality
{
    Best,
    Q2160,
    Q1440,
    Q1080,
    Q720,
    Q480,
    Q360,
};

enum class Container
{
    Mp4,
    Mkv,
    Webm,
};

enum class AudioFormat
{
    Best, ///< keep the source codec (m4a/opus)
    Mp3,
    M4a,
    Opus,
    Flac,
    Wav,
};

/// What the download dialog offers first (its "Video / Audio only / Advanced"
/// choice); remembered from the last download so audio-only users stay there.
enum class DownloadKind
{
    Video,
    Audio,
    Custom,
};

enum class FilenamePattern
{
    Title,        ///< Title.ext
    TitleId,      ///< Title [id].ext
    ChannelTitle, ///< Channel - Title.ext
};

/// What SponsorBlock does with a category (FEATURES B6).
enum class SponsorAction
{
    Skip,
    Show, ///< only mark on the progress bar
    Off,
};

/// Typed facade over QSettings. The only place in the code base allowed to
/// construct a QSettings. Defaults live in one table (settings.cpp), every
/// setter emits a change signal so the rest of the app reacts instead of
/// polling.
class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Settings)

public:
    explicit Settings(QObject* parent = nullptr);
    Settings(const QString& iniFilePath, QObject* parent = nullptr);
    ~Settings() override;

    // window/
    [[nodiscard]] QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    [[nodiscard]] QByteArray windowState() const;
    void setWindowState(const QByteArray& state);
    [[nodiscard]] QByteArray settingsDialogGeometry() const;
    void setSettingsDialogGeometry(const QByteArray& geometry);
    [[nodiscard]] CloseAction closeAction() const;
    void setCloseAction(CloseAction action);
    [[nodiscard]] bool showRail() const;
    void setShowRail(bool show);
    /// The rail shows a label next to each icon (wide rail) or icons only.
    [[nodiscard]] bool railLabels() const;
    void setRailLabels(bool show);
    [[nodiscard]] AppMode lastMode() const;
    void setLastMode(AppMode mode);
    [[nodiscard]] QString lastUrl() const;
    void setLastUrl(const QString& url);
    /// Music mode keeps its own "where I left off" page.
    [[nodiscard]] QString lastMusicUrl() const;
    void setLastMusicUrl(const QString& url);
    [[nodiscard]] bool downloadsPanelOpen() const;
    void setDownloadsPanelOpen(bool open);

    // general/
    [[nodiscard]] StartMode startMode() const;
    void setStartMode(StartMode mode);
    /// The mode a launch opens in: startMode() with LastUsed resolved.
    [[nodiscard]] AppMode startupMode() const;
    /// Middle-click on the page starts scrolling (public issue #17), a
    /// Chromium switch read at start. Off by default: on Linux middle-click
    /// pastes.
    [[nodiscard]] bool middleClickScroll() const;
    void setMiddleClickScroll(bool enabled);
    /// A two-finger touchpad swipe goes back and forward (public issue #18, ADR-022).
    /// On by default; never in TV mode.
    [[nodiscard]] bool swipeNavigation() const;
    void setSwipeNavigation(bool enabled);
    /// "More apps from Ktechpit" in About (KtechpitApps module, ADR-025). On by default.
    [[nodiscard]] bool showOtherApps() const;
    void setShowOtherApps(bool show);
    [[nodiscard]] StartPage startPage() const;
    void setStartPage(StartPage page);
    [[nodiscard]] bool notifyOnDownloadFinish() const;
    void setNotifyOnDownloadFinish(bool enabled);
    [[nodiscard]] bool trayEnabled() const;
    void setTrayEnabled(bool enabled);

    // appearance/
    [[nodiscard]] Theme theme() const;
    void setTheme(Theme theme);
    [[nodiscard]] double zoomFactor() const;
    void setZoomFactor(double factor);
    [[nodiscard]] double interfaceScale() const;
    void setInterfaceScale(double scale);
    static constexpr double kMinInterfaceScale = 0.5;
    static constexpr double kMaxInterfaceScale = 3.0;

    // playback/
    [[nodiscard]] bool autoplay() const;
    void setAutoplay(bool enabled);
    [[nodiscard]] CodecPreference codecPreference() const;
    void setCodecPreference(CodecPreference codec);
    [[nodiscard]] bool hardwareVideoDecode() const;
    void setHardwareVideoDecode(bool enabled);
    [[nodiscard]] bool keepScreenAwake() const;
    /// Playback position as a progress bar behind the taskbar entry (off by default).
    [[nodiscard]] bool taskbarProgress() const;
    void setTaskbarProgress(bool enabled);
    void setKeepScreenAwake(bool enabled);
    [[nodiscard]] MiniPlayerSize miniPlayerSize() const;
    void setMiniPlayerSize(MiniPlayerSize size);
    /// Keep playing when the window is minimized/hidden (LESSONS V8).
    [[nodiscard]] bool playInBackground() const;
    void setPlayInBackground(bool enabled);

    // blocking/
    [[nodiscard]] bool blockAds() const;
    void setBlockAds(bool enabled);
    [[nodiscard]] bool blockTrackers() const;
    void setBlockTrackers(bool enabled);
    [[nodiscard]] bool hideShorts() const;
    void setHideShorts(bool enabled);
    /// Return YouTube Dislike counts under the dislike button (FEATURES D14).
    [[nodiscard]] bool showDislikes() const;
    void setShowDislikes(bool enabled);
    [[nodiscard]] bool sponsorBlockEnabled() const;
    void setSponsorBlockEnabled(bool enabled);
    /// SponsorBlock category id (e.g. "sponsor") → action. Unknown ids get the default.
    [[nodiscard]] SponsorAction sponsorAction(const QString& category) const;
    void setSponsorAction(const QString& category, SponsorAction action);
    [[nodiscard]] QHash<QString, SponsorAction> sponsorActions() const;
    [[nodiscard]] bool sponsorBlockMarkers() const;
    void setSponsorBlockMarkers(bool enabled);
    /// Lifetime blocked-request counter (persisted by the interceptor owner, FEATURES B5).
    [[nodiscard]] quint64 blockedCount() const;
    void setBlockedCount(quint64 count);

    // downloads/
    [[nodiscard]] QString downloadDirectory() const;
    void setDownloadDirectory(const QString& directory);
    [[nodiscard]] FilenamePattern filenamePattern() const;
    void setFilenamePattern(FilenamePattern pattern);
    /// Downloads sorted into Videos / Music / Playlists / Channels (default on).
    [[nodiscard]] bool organiseDownloads() const;
    void setOrganiseDownloads(bool enabled);
    [[nodiscard]] VideoQuality defaultQuality() const;
    void setDefaultQuality(VideoQuality quality);
    [[nodiscard]] Container defaultContainer() const;
    void setDefaultContainer(Container container);
    [[nodiscard]] AudioFormat defaultAudioFormat() const;
    void setDefaultAudioFormat(AudioFormat format);
    [[nodiscard]] DownloadKind lastDownloadKind() const;
    void setLastDownloadKind(DownloadKind kind);
    [[nodiscard]] int concurrentDownloads() const;
    void setConcurrentDownloads(int count);
    static constexpr int kMaxConcurrentDownloads = 5;
    /// 0 = unlimited.
    [[nodiscard]] int speedLimitKbps() const;
    void setSpeedLimitKbps(int kbps);
    [[nodiscard]] bool useSessionCookies() const;
    void setUseSessionCookies(bool enabled);
    [[nodiscard]] bool embedThumbnail() const;
    void setEmbedThumbnail(bool enabled);
    [[nodiscard]] bool embedMetadata() const;
    void setEmbedMetadata(bool enabled);
    [[nodiscard]] bool removeSponsors() const;
    void setRemoveSponsors(bool enabled);
    /// Preferred subtitle languages for the dialog's default (e.g. {"en"}).
    [[nodiscard]] QStringList subtitleLanguages() const;
    void setSubtitleLanguages(const QStringList& languages);

    // engine/
    [[nodiscard]] bool engineAutoUpdate() const;
    void setEngineAutoUpdate(bool enabled);
    [[nodiscard]] bool engineUseSystem() const;
    void setEngineUseSystem(bool enabled);
    [[nodiscard]] QString engineSystemPath() const;
    void setEngineSystemPath(const QString& path);
    [[nodiscard]] QDateTime engineLastCheck() const;
    void setEngineLastCheck(const QDateTime& when);

    // tv/
    [[nodiscard]] bool tvFullscreen() const;
    void setTvFullscreen(bool enabled);
    [[nodiscard]] bool tvGamepad() const;
    void setTvGamepad(bool enabled);
    [[nodiscard]] bool tvHideShorts() const;
    void setTvHideShorts(bool enabled);
    [[nodiscard]] bool tvUnlockResolution() const;
    void setTvUnlockResolution(bool enabled);
    [[nodiscard]] bool tvLowMemory() const;
    void setTvLowMemory(bool enabled);

    // advanced/
    [[nodiscard]] HardwareAcceleration hardwareAcceleration() const;
    void setHardwareAcceleration(HardwareAcceleration mode);
    [[nodiscard]] bool gpuAutoDisabled() const;
    void setGpuAutoDisabled(bool disabled);
    [[nodiscard]] int gpuProbeStrikes() const;
    void setGpuProbeStrikes(int strikes);
    [[nodiscard]] bool gpuFallbackNotice() const;
    void setGpuFallbackNotice(bool pending);
    /// Pending "hardware video decoding was turned off" notice (GPU storm
    /// with the experimental decoder on: that switch is backed out first).
    [[nodiscard]] bool videoDecodeFallbackNotice() const;
    void setVideoDecodeFallbackNotice(bool pending);
    /// The app version whose release notes were shown ("What's new"); empty = never.
    /// The welcome sheet was shown (once, on the first start of a new install).
    [[nodiscard]] bool welcomeSeen() const;
    void setWelcomeSeen(bool seen);
    /// Neither the welcome nor any release notes were ever shown: a new install
    /// (or a Red 9 upgrade, whose settings do not carry over).
    [[nodiscard]] bool isFirstRun() const;
    [[nodiscard]] QString whatsNewSeenVersion() const;
    void setWhatsNewSeenVersion(const QString& version);
    /// The free tier's download counter (day + count); see services::LicenseService.
    [[nodiscard]] DailyAllowance freeDownloads() const;
    void setFreeDownloads(const DailyAllowance& allowance);
    [[nodiscard]] QString userAgentOverride() const;
    void setUserAgentOverride(const QString& userAgent);
    [[nodiscard]] SignInUserAgent signInUserAgent() const;
    void setSignInUserAgent(SignInUserAgent agent);

    void sync();
    void resetToDefaults();
    [[nodiscard]] QString fileName() const;

Q_SIGNALS:
    void closeActionChanged(red::core::CloseAction action);
    void showRailChanged(bool show);
    void railLabelsChanged(bool show);
    void startModeChanged(red::core::StartMode mode);
    void startPageChanged(red::core::StartPage page);
    void middleClickScrollChanged(bool enabled);
    void swipeNavigationChanged(bool enabled);
    void showOtherAppsChanged(bool show);
    void notifyOnDownloadFinishChanged(bool enabled);
    void trayEnabledChanged(bool enabled);
    void themeChanged(red::core::Theme theme);
    void zoomFactorChanged(double factor);
    void interfaceScaleChanged(double scale);
    void autoplayChanged(bool enabled);
    void codecPreferenceChanged(red::core::CodecPreference codec);
    void hardwareVideoDecodeChanged(bool enabled);
    void keepScreenAwakeChanged(bool enabled);
    void taskbarProgressChanged(bool enabled);
    void miniPlayerSizeChanged(red::core::MiniPlayerSize size);
    void playInBackgroundChanged(bool enabled);
    void blockingChanged(); ///< ads / trackers toggled
    void hideShortsChanged(bool enabled);
    void showDislikesChanged(bool enabled);
    void sponsorBlockChanged(); ///< enabled, categories or markers
    void downloadDirectoryChanged(const QString& directory);
    void downloadDefaultsChanged(); ///< any downloads/ default that the dialog reads
    void concurrentDownloadsChanged(int count);
    void speedLimitChanged(int kbps);
    void engineConfigChanged(); ///< auto-update / system path
    void tvConfigChanged();
    void hardwareAccelerationChanged(red::core::HardwareAcceleration mode);
    void gpuAutoDisabledChanged(bool disabled);
    void userAgentChanged();

private:
    [[nodiscard]] bool boolValue(QLatin1StringView key, bool def) const;
    void migrateGeneralGroup();
    bool storeBool(QLatin1StringView key, bool def, bool value);
    [[nodiscard]] int intValue(QLatin1StringView key, int def) const;
    bool storeInt(QLatin1StringView key, int def, int value);
    [[nodiscard]] QString stringValue(QLatin1StringView key, const QString& def = {}) const;
    bool storeString(QLatin1StringView key, const QString& value);

    std::unique_ptr<QSettings> m_store;
};

/// Default action for a SponsorBlock category; the canonical category list.
[[nodiscard]] QStringList sponsorCategories();
[[nodiscard]] SponsorAction defaultSponsorAction(const QString& category);

} // namespace red::core

Q_DECLARE_METATYPE(red::core::Theme)
Q_DECLARE_METATYPE(red::core::CloseAction)
Q_DECLARE_METATYPE(red::core::StartPage)
Q_DECLARE_METATYPE(red::core::AppMode)
Q_DECLARE_METATYPE(red::core::CodecPreference)
Q_DECLARE_METATYPE(red::core::MiniPlayerSize)
Q_DECLARE_METATYPE(red::core::HardwareAcceleration)
Q_DECLARE_METATYPE(red::core::VideoQuality)
Q_DECLARE_METATYPE(red::core::Container)
Q_DECLARE_METATYPE(red::core::AudioFormat)
Q_DECLARE_METATYPE(red::core::DownloadKind)
Q_DECLARE_METATYPE(red::core::FilenamePattern)
Q_DECLARE_METATYPE(red::core::SponsorAction)
Q_DECLARE_METATYPE(red::core::SignInUserAgent)
