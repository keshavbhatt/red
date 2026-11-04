# FEATURES — the scope contract

Decision legend: **KEEP** (in v10), **LATER** (after v10), **DROP** (never).
Status legend: `todo`, `wip`, `done (class)`, `verified` (exercised live on the owner's desktop).

Sources: `reference/analysis-red-v9.md` (Red v9), TV-mode research (LESSONS section B),
new-whatsie `DOCS/FEATURES.md` (desktop-shell features that proved themselves).

The owner's brief (2026-09-06): "smart, performant", two modes (desktop YouTube + TV mode on
Leanback; a third, YouTube Music, was added on 2026-09-14, section G), fast ad blocking, out-of-the-box yt-dlp downloads for anything on YouTube, UI/UX from
scratch with a brand theme matching YouTube, whatsie coding practices, packaging last.

## A. Shell (window, tray, identity)

| # | Feature | Red v9 | Decision | Status / class |
|---|---|---|---|---|
| S1 | Single main window, YouTube fills it, native chrome is a slim **left rail** (see DESIGN.md), icons only or with labels (default), F10 for its menu | side panel + splitter | KEEP | done, `ui::MainWindow`, `ui::SideRail`, `ui::RailButton`, ADR-015 |
| S2 | Persist window geometry/state; restore last page (setting) | yes | KEEP | done — `MainWindow` |
| S3 | Single instance; second launch forwards URL / `--tv` / `--download` commands | RunGuard, no forwarding | KEEP | done — `app::SingleInstance` (from whatsie) |
| S4 | System tray: show/hide, play/pause, next, downloads, TV mode, quit; close-to-tray optional (default quit) | none | KEEP | done — `ui::TrayController` |
| S5 | Fullscreen (F11 + page requests), hint overlay on enter | yes | KEEP | done — `MainWindow`, `web::FullScreenHint` |
| S6 | Native notifications (download finished with thumbnail + actions) | none | KEEP | done — `platform::FreedesktopNotifier` |
| S7 | Crash handler + log file + diagnostics copy | none | KEEP | from whatsie |
| S8 | GPU auto-fallback, Wayland→XCB retry | none | KEEP | from whatsie (ADR-032 lineage) |
| S12 | "What's new" once per version on first start, notes from the bundled CHANGELOG.md; on demand from About's What's new link, with a version picker over every release | intro slides | KEEP | done, `core::changelogSection`, `core::changelogReleases`, `ui::WhatsNewDialog`, `RED_DEBUG_OPEN=whatsnew`; a new install gets `ui::WelcomeDialog` instead (ADR-018) |
| S13 | Playback position as a progress bar behind the taskbar entry (Unity LauncherEntry signal, Plasma draws it), off by default | none | KEEP | done, `platform::TaskbarProgress`, Settings > Playback |
| S9 | App lock, licensing, RateApp, intro slides, history tab | yes | DROP | Red stays closed source, but these were never worth their upkeep; YouTube keeps history |
| S10 | `red <youtube-url>` / `red --tv` / `red --download <url>` CLI | Open URL dialog | KEEP | done — `app::CliOptions` |
| S11 | External links (non-Google) open in the system browser; Google sign-in pop-ups stay in-app | none | KEEP | done — `core::navigation_policy`, `web::PopupWindow` |
| S14 | "More apps from Ktechpit": two random cards in About (trending apps more often), the full list with Refresh behind *See all*, off in Settings, General | none | KEEP | done, `KtechpitApps` module (subtree), ADR-025 |

## B. Desktop YouTube mode

| # | Feature | Red v9 | Decision | Status / class |
|---|---|---|---|---|
| D1 | Theme System/Light/Dark for chrome **and** page (page via `html[dark]` + `prefers-color-scheme`) | PREF cookie + reload | KEEP | done — `core::ThemeService`, `theme-control.js` |
| D2 | Page zoom (Ctrl +/-/0) and interface scale | none | KEEP | done — `core::zoom_policy` |
| D3 | Autoplay without gesture (setting) | yes | KEEP | profile attribute |
| D4 | Preferred video codec (Auto / H.264 / VP9 / AV1) — h264ify-style, big CPU saver | none | KEEP | done — `codec-preference.js` |
| D5 | Hardware video decoding toggle (VA-API flags, restart) | none | KEEP | done — `core::chromium_flags` |
| D6 | **Mini player**: compact always-on-top window with the player fullbleed (Ctrl+Shift+M) | none | KEEP | done, `MainWindow::setMiniPlayer`, `mini-player.js`; only offered while a video is on screen (2026-09-07); close button, Esc and a corner restore button return to the full window; Shorts get a 9:16 window, YouTube Music a tall one (`core::mini_player_policy`, ADR-016) (2026-10-16) |
| D7 | MPRIS media controls (media keys, KDE/GNOME applets): play/pause/next/prev/seek, metadata + artwork | none | KEEP | done — `platform::MprisService`, `media-session.js` |
| D8 | Keep screen awake while playing (Chromium wake lock; own inhibit as fallback) | none | KEEP | done — `platform::ScreenInhibitor` |
| D9 | Hide Shorts (feed + guide) | none | KEEP | done — `hide-shorts.js` + response filter |
| D10 | Theater mode forced | yes | DROP | YouTube remembers the user's choice itself |
| D11 | Custom scrollbars, no text selection | yes | DROP | fights the page |
| D12 | Google sign-in works (sanitized Chrome UA; optional Firefox UA fallback setting) | Firefox UA | KEEP | done — `web::user_agent` |
| D13 | Custom error page with retry | yes | KEEP | done — `web::error_page` (from whatsie) |
| D14 | Return YouTube Dislike counts (setting, default on) | none | KEEP | done — `services::DislikesClient`, `dislikes.js` |
| D15 | DeArrow titles/thumbnails | none | LATER | CSP img-src on desktop |
| D16 | Two-finger touchpad swipe for back / forward (setting, default on; YouTube and YouTube Music, never TV mode; public issue #18) | n/a | KEEP | done, `core::SwipeTracker`, `web::SwipeIndicator`, ADR-022 |

## C. TV mode (Leanback)

| # | Feature | Prior art | Decision | Status / class |
|---|---|---|---|---|
| T1 | Load `youtube.com/tv` with the PS4/Cobalt UA split (navigator vs wire) | yes | KEEP | done — `web::TvMode`, `web::RequestInterceptor` |
| T2 | Switch modes at runtime (rail mode switch, tray, Ctrl+3 / Ctrl+T toggle, `--tv`, *Back to YouTube* pill); "Open Red in" start mode (YouTube default, or last used) | n/a | KEEP | done, `ui::MainWindow`, `ui::SideRail`, `ui::TvExitButton`, ADR-014 |
| T3 | 16:9 content, fullscreen by default in TV mode, window chrome hidden | yes | KEEP | done — `MainWindow` |
| T4 | Keyboard: arrows/Enter/Esc/Backspace native; right-click = back; hotkeys copy URL, captions, volume, speed | yes | KEEP | done — `tv/input.js` |
| T5 | Gamepad → key events (Standard Gamepad map, repeat, stick); A held like a remote's OK, so a hold opens Leanback's video menu (#25) | yes | KEEP | done — `tv/gamepad.js` |
| T6 | `window.h5vcc` shim (deep link, resolution override/unlock 4K) | yes | KEEP | done — `tv/h5vcc.js` |
| T7 | Identification rewrite (environment/ytcfg/tectonicConfig/`context.client` → DESKTOP Chrome "Red") | yes | KEEP | done — `tv/identification.js` |
| T8 | Fix exit / reload / playback-cap / sign-in / DRM notice signals via `resolveCommand` hook | yes | KEEP | done — `tv/leanback-fixes.js` |
| T9 | Feed ads + video ads + shorts ads stripped from InnerTube responses (shared with desktop) | yes | KEEP | done — `adblock.js` |
| T10 | SponsorBlock in TV mode (segments from C++, native Leanback toast) | yes | KEEP | done — `sponsorblock.js` |
| T11 | Hide Shorts, guide tabs filter, low-memory mode, super-resolution removal | yes | KEEP (hide shorts, low-memory) / LATER (others) | hide shorts done; open notes: `/tv` is iframed in rare cases, so a preload may need to run in subframes (Red keeps `runsOnSubFrames` off until that is observed); a "Red" entry inside Leanback's own Settings page is LATER |
| T12 | DIAL / device discoverability (cast from phone) | yes | LATER | needs SSDP server |
| T13 | Touch overlay, userstyles, music mode | yes | DROP | the "music mode" seen elsewhere is an audio-only flag inside Leanback; Red's Music mode is the YouTube Music site itself, section G |
| T14 | Download the video being watched in TV mode (Ctrl+D) | n/a | KEEP | done — `tv/current-video.js` |
| T15 | Voice search (`env_enableMediaStreams`), mic permission | yes | KEEP | done — `tv/leanback-fixes.js`, permissions |

## D. Blocking

| # | Feature | Red v9 | Decision | Status / class |
|---|---|---|---|---|
| B1 | **Network layer**: O(1) host-suffix set of ad/tracker domains + youtube.com ad path prefixes; never googlevideo; no per-request settings/signals | 630 substring rules, QSettings write per block | KEEP | done — `core::BlockList`, `web::RequestInterceptor` |
| B2 | **Response layer**: `JSON.parse`/`fetch`/XHR/`ytInitialPlayerResponse` hooks strip `adPlacements`, `adSlots`, `playerAds`, feed `adSlotRenderer`s, masthead, promoted | none | KEEP | done — `adblock.js` |
| B3 | **Cosmetic layer**: CSS for ad containers, enforcement (anti-adblock) dialog, skip-button + ad fast-forward via a player-scoped MutationObserver | 300 ms polling clicker | KEEP | done — `adblock.js`, `adblock.css` |
| B4 | Block YouTube trackers (`log_event`, `ptracking`, `api/stats/*`) — separate toggle | yes | KEEP | done — `core::BlockList` |
| B5 | Blocked-request counter shown in Settings (no log window) | log window | KEEP | atomic counter |
| B6 | SponsorBlock: per-category skip/mark/off, progress-bar markers, toast, k-anonymity API | none | KEEP | done — `core::SponsorBlockClient`, `sponsorblock.js` |
| B7 | Remote filter lists (EasyList) | none | DROP | keeps the interceptor tiny; lists are curated |

## E. Downloads (yt-dlp)

| # | Feature | Red v9 | Decision | Status / class |
|---|---|---|---|---|
| E1 | Engine provisioning: `yt-dlp_linux` standalone + QuickJS runtime + ffmpeg (system or downloaded), SHA-256 verified, progress UI, first-use auto-setup | `python3 core` zipimport, no JS runtime, no ffmpeg check | KEEP | done — `core::EngineManager`, `ui::EngineSetupDialog` |
| E2 | Update check (tag date compare), manual update, daily auto-check (setting) | broken date compare | KEEP | done — `core::engine_version` |
| E3 | Probe: `-J` (single) / `-J --flat-playlist` (playlist/channel/tab); parsed into typed `MediaInfo` | server-side PHP metadata | KEEP | done — `core::MediaProbe`, `core::media_info` |
| E4 | Download dialog: hero (thumb/title/channel/duration), presets (quality, container), audio-only formats, subtitles, embed thumb/metadata/chapters, SponsorBlock remove, playlist entry picker, folder, filename preview | radio list of raw formats | KEEP | done — `ui::DownloadDialog`; Video / Audio only / Advanced as cards, last kind remembered, "Download audio only…" in the context menu (2026-09-07) |
| E5 | Advanced format table (all formats: codec, fps, HDR, size) with explicit video+audio pick | partial | KEEP | done — `ui::FormatTable` |
| E6 | Queue with concurrency, pause (kill + `--continue`), resume, cancel, retry, remove, open, show in folder, clear finished; persisted across restarts | flat files, kill = pause | KEEP | done — `core::DownloadJob`, `core::DownloadQueue`, `web::…` n/a |
| E7 | Progress via `--newline --progress-template "download:%(progress)j"` and `postprocess:` stage; final path via `--print after_move:filepath` | regex on human output | KEEP | done — `core::ytdlp_output` |
| E8 | Cookies from the app's own YouTube session (Netscape file, 0600, deleted after) — setting | none | KEEP | done — `web::CookieExporter`, `core::netscape_cookies` |
| E9 | Speed limit, output template presets, thumbnail/metadata embed defaults; downloads sorted into Videos / Music / Playlists / Channels (switch, default on) | none | KEEP | settings, `core::downloadFolder`, `DownloadOptions::folder` |
| E10 | Downloads panel: slide-in right panel with cards (thumb, title, status, progress, hover actions), engine chip, "New download" | side panel list | KEEP | done — `ui::DownloadsPanel`, `ui::DownloadCardDelegate` |
| E11 | Download from context menu on video links / thumbnails, from the rail button (current page), from Ctrl+D, from `--download` | context menu | KEEP | done — `web::WebPage`, `ui::MainWindow` |
| E12 | Watch/Listen in mpv | yes | DROP | the app is the player |
| E13 | Notification on completion with Open / Show in folder | none | KEEP | done — `ui::DownloadsHub` |
| E14 | Live streams (record from now), premieres | none | LATER | |

## G. Music mode (music.youtube.com)

| # | Feature | Status |
|---|---|---|
| M1 | Third `AppMode`: rail mode switch, Ctrl+2 / Ctrl+M toggle, `red --music`, desktop action, "Open Red in" choice (ADR-014), own "where I left off" (`window/lastMusicUrl`) | done, ADR-013 |
| M2 | YouTube Music links (masthead app picker, `target=_blank`) open in the main view instead of a pop-up | done, `WebPage` `newWindowRequested`, pop-up first-navigation safety net |
| M3 | Media keys, tray now-playing, MPRIS with album and the music.youtube.com URL; reporting continues on every page | done, `media-session.js` (no "parked" rule in Music, `ytmusic-player-bar` transport) |
| M4 | Background play, ad and tracker blocking, SponsorBlock share the desktop scripts | done, common script set only |
| M5 | Downloads preselect *Audio only* without changing the desktop default; Ctrl+D downloads the playing track from any page | done, `DownloadDialog::preselectKind`, `WebView::downloadableUrl` media fallback |
| M6 | Mini player as a tall window around YouTube Music's own responsive player page (ADR-016), screen not kept awake, page background dark (YouTube Music is dark-only) | done, `music/mini-player.js` |
| M9 | The Premium-only *Download* entry in YouTube Music's menus (song rows, queue, player bar) downloads through Red: the video or playlist id comes from the neighbouring queue entries | done, `music/download-menu.js` |
| M7 | Album pages (`/browse/MPREb_`) downloadable | LATER |
| M8 | SponsorBlock markers on YouTube Music's own progress bar; segments off the watch page | LATER |

## F. Settings surface

Target: ≤ 35 options, sidebar navigation with 7 pages (General, Appearance, Playback, Blocking,
Downloads, TV mode, Advanced). Restart-required: interface scale, hardware acceleration, hardware
video decoding, user-agent override.
