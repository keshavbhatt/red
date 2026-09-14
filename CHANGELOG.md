# Changelog

All notable changes to Red. The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [Semantic Versioning](https://semver.org/).

## [10.1.0] - 2026-09-14

### Added
- **Music mode**: YouTube Music in the same window (Ctrl+M, the rail button, `red --music`),
  remembered across launches like TV mode. Music keeps playing while you browse, with media keys,
  the tray and desktop media controls showing album and cover, and *Audio only* preselected for
  downloads. Links to YouTube Music open here instead of in a separate window.
- **What's new** sheet on the first start of a new version, with these notes.
- **Online guide** in the menu and in About, and a **Report a bug** sheet that opens a pre-filled
  issue or an email with the diagnostics on the clipboard.
- Optional playback progress bar behind Red's taskbar entry (Settings, Playback).
- Upsell pop-ups such as "Try YouTube Music family plan" are removed with the ads.

### Fixed
- F11 toggles full screen in the snap and Flatpak builds too.
- Dialogs raised by pop-up windows (YouTube Music, sign-in) use Red's own sheets.
- YouTube Music no longer claims "No internet connection": its connectivity probe was blocked.
- Long drop-down lists (subtitle languages) scroll instead of running off the screen.
- A long title no longer stretches the tray menu.

## [10.0.0] - 2026-09-07

Red 10 is a redesigned Red on Qt 6.11 and Qt WebEngine, taking over from the Qt 5 based Red 9.

### Added
- **Two modes in one window**: the desktop site and YouTube's living-room TV interface
  (Ctrl+T), remembered across launches, with the current video carried across the switch.
- **Blocking** that never touches playback hosts: ads stripped from player and feed responses
  before YouTube renders them plus a small network-level list; SponsorBlock with per-category
  skip / mark / off; Return YouTube Dislike; Hide Shorts (desktop and TV).
- **Downloads** on a self-provisioning download engine (fetched from its official releases,
  checksum-verified, updated daily; the media converter comes from the system): video, audio
  only or exact streams, playlists and channels with entry picking, subtitles, embedded
  thumbnail, metadata and chapters, sponsor-segment removal, a queue with pause / resume /
  retry, notifications with *Show in folder*, and the app's own YouTube sign-in reused for
  age-restricted or rate-limited videos. Playlists are detected from any watch link.
- **Desktop integration**: media keys, MPRIS, a tray icon with playback controls, screen kept
  awake while playing, a mini player, single instance (`red <link>`, `red --download <link>`),
  desktop actions for TV mode, downloads and settings.
- **Sign-in** through Google's page inside the app, with the identity Google accepts.
- **New design**: a side rail, a downloads panel, and dialogs that follow YouTube's
  light and dark themes (system, light or dark, applied to the page too).
- **TV mode extras**: gamepad support, keyboard shortcuts for volume, speed and captions,
  4K unlock on smaller displays, low-memory mode, voice search.
- Snap (kde-neon-6) and Flatpak (KDE 6.11 runtime) packaging, built in CI.

### Changed
- Settings live in `~/.config/ktechpit/red.conf`; data in `~/.local/share/ktechpit/red/`.
  Red 9 settings are not migrated.

### Removed
- Red 9's app lock, rate-app prompts, intro slides, history tab, forced theater
  mode, custom scrollbars, "watch in mpv", and the remote filter lists.
