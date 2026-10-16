#pragma once

#include <QLatin1StringView>

// Every persisted key lives here exactly once. Format: "section/camelCase".
// Adding a key = adding a typed accessor pair on core::Settings + a test.
namespace red::core::keys {

// window/
inline constexpr QLatin1StringView kWindowGeometry{"window/geometry"};
inline constexpr QLatin1StringView kWindowState{"window/state"};
inline constexpr QLatin1StringView kSettingsDialogGeometry{"window/settingsDialogGeometry"};
inline constexpr QLatin1StringView kCloseAction{"window/closeAction"};
inline constexpr QLatin1StringView kShowRail{"window/showRail"};
inline constexpr QLatin1StringView kWhatsNewSeenVersion{"window/whatsNewSeenVersion"};
inline constexpr QLatin1StringView kFreeDownloadsDay{"licensing/freeDownloadsDay"};
inline constexpr QLatin1StringView kFreeDownloadsUsed{"licensing/freeDownloadsUsed"};
inline constexpr QLatin1StringView kLastMode{"window/lastMode"};
inline constexpr QLatin1StringView kLastUrl{"window/lastUrl"};
inline constexpr QLatin1StringView kLastMusicUrl{"window/lastMusicUrl"};
inline constexpr QLatin1StringView kDownloadsPanelOpen{"window/downloadsPanelOpen"};

// general/
inline constexpr QLatin1StringView kStartMode{"general/startMode"};
inline constexpr QLatin1StringView kStartPage{"general/startPage"};
inline constexpr QLatin1StringView kNotifyOnDownloadFinish{"general/notifyOnDownloadFinish"};
inline constexpr QLatin1StringView kTrayEnabled{"general/trayEnabled"};

// appearance/
inline constexpr QLatin1StringView kTheme{"appearance/theme"};
inline constexpr QLatin1StringView kZoomFactor{"appearance/zoomFactor"};
inline constexpr QLatin1StringView kInterfaceScale{"appearance/interfaceScale"};

// playback/
inline constexpr QLatin1StringView kAutoplay{"playback/autoplay"};
inline constexpr QLatin1StringView kCodecPreference{"playback/codecPreference"};
inline constexpr QLatin1StringView kHardwareVideoDecode{"playback/hardwareVideoDecode"};
inline constexpr QLatin1StringView kKeepScreenAwake{"playback/keepScreenAwake"};
inline constexpr QLatin1StringView kTaskbarProgress{"playback/taskbarProgress"};
inline constexpr QLatin1StringView kMiniPlayerSize{"playback/miniPlayerSize"};
inline constexpr QLatin1StringView kPlayInBackground{"playback/playInBackground"};

// blocking/
inline constexpr QLatin1StringView kBlockAds{"blocking/ads"};
inline constexpr QLatin1StringView kBlockTrackers{"blocking/trackers"};
inline constexpr QLatin1StringView kHideShorts{"blocking/hideShorts"};
inline constexpr QLatin1StringView kSponsorBlockEnabled{"blocking/sponsorBlock"};
inline constexpr QLatin1StringView kSponsorBlockCategories{"blocking/sponsorBlockCategories"};
inline constexpr QLatin1StringView kSponsorBlockMarkers{"blocking/sponsorBlockMarkers"};
inline constexpr QLatin1StringView kBlockedCount{"blocking/blockedCount"};
inline constexpr QLatin1StringView kShowDislikes{"blocking/showDislikes"};

// downloads/
inline constexpr QLatin1StringView kDownloadDirectory{"downloads/directory"};
inline constexpr QLatin1StringView kFilenamePattern{"downloads/filenamePattern"};
inline constexpr QLatin1StringView kOrganiseDownloads{"downloads/organise"};
inline constexpr QLatin1StringView kDefaultQuality{"downloads/defaultQuality"};
inline constexpr QLatin1StringView kDefaultContainer{"downloads/defaultContainer"};
inline constexpr QLatin1StringView kDefaultAudioFormat{"downloads/defaultAudioFormat"};
inline constexpr QLatin1StringView kLastDownloadKind{"downloads/lastKind"};
inline constexpr QLatin1StringView kConcurrentDownloads{"downloads/concurrent"};
inline constexpr QLatin1StringView kSpeedLimitKbps{"downloads/speedLimitKbps"};
inline constexpr QLatin1StringView kUseSessionCookies{"downloads/useSessionCookies"};
inline constexpr QLatin1StringView kEmbedThumbnail{"downloads/embedThumbnail"};
inline constexpr QLatin1StringView kEmbedMetadata{"downloads/embedMetadata"};
inline constexpr QLatin1StringView kRemoveSponsors{"downloads/removeSponsors"};
inline constexpr QLatin1StringView kSubtitleLanguages{"downloads/subtitleLanguages"};

// engine/
inline constexpr QLatin1StringView kEngineAutoUpdate{"engine/autoUpdate"};
inline constexpr QLatin1StringView kEngineUseSystem{"engine/useSystem"};
inline constexpr QLatin1StringView kEngineSystemPath{"engine/systemPath"};
inline constexpr QLatin1StringView kEngineLastCheck{"engine/lastCheck"};

// tv/
/// Before 10.3 "Start Red in TV mode"; read once to seed general/startMode.
inline constexpr QLatin1StringView kLegacyTvStartInTvMode{"tv/startInTvMode"};
inline constexpr QLatin1StringView kTvFullscreen{"tv/fullscreen"};
inline constexpr QLatin1StringView kTvGamepad{"tv/gamepad"};
inline constexpr QLatin1StringView kTvHideShorts{"tv/hideShorts"};
inline constexpr QLatin1StringView kTvUnlockResolution{"tv/unlockResolution"};
inline constexpr QLatin1StringView kTvLowMemory{"tv/lowMemory"};

// advanced/
inline constexpr QLatin1StringView kHardwareAcceleration{"advanced/hardwareAcceleration"};
inline constexpr QLatin1StringView kGpuAutoDisabled{"advanced/gpuAutoDisabled"};
inline constexpr QLatin1StringView kGpuProbeStrikes{"advanced/gpuProbeStrikes"};
inline constexpr QLatin1StringView kGpuFallbackNotice{"advanced/gpuFallbackNotice"};
inline constexpr QLatin1StringView kVideoDecodeFallbackNotice{"playback/videoDecodeFallbackNotice"};
inline constexpr QLatin1StringView kUserAgentOverride{"advanced/userAgentOverride"};
inline constexpr QLatin1StringView kSignInUserAgent{"advanced/signInUserAgent"};

} // namespace red::core::keys
