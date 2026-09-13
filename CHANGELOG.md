# Changelog

All notable changes to Red. The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [Semantic Versioning](https://semver.org/).

## [10.0.0] — unreleased

Red 10 is a complete rewrite of Red on Qt 6.11 and Qt WebEngine (C++20, CMake), replacing the
Qt 5 code base of Red 9. Nothing of the old code was kept.

### Added
- **Two modes in one window**: the desktop site and YouTube's living-room TV interface
  (Ctrl+T), remembered across launches, with the current video carried across the switch.
- **Blocking** that never touches playback hosts: ads stripped from player and feed responses
  before YouTube renders them plus a small network-level list; SponsorBlock with per-category
  skip / mark / off; Return YouTube Dislike; Hide Shorts (desktop and TV).
- **Downloads** on a self-provisioning engine (yt-dlp plus its script runtime, fetched from the
  official releases, checksum-verified, updated daily; ffmpeg from the system): video, audio
  only or exact streams, playlists and channels with entry picking, subtitles, embedded
  thumbnail, metadata and chapters, sponsor-segment removal, a queue with pause / resume /
  retry, notifications with *Show in folder*, and the app's own YouTube sign-in reused for
  age-restricted or rate-limited videos. Playlists are detected from any watch link.
- **Desktop integration**: media keys, MPRIS, a tray icon with playback controls, screen kept
  awake while playing, a mini player, single instance (`red <link>`, `red --download <link>`),
  desktop actions for TV mode, downloads and settings.
- **Sign-in** through Google's page inside the app, with the identity Google accepts.
- **Design** from scratch: a side rail, a downloads panel, and dialogs that follow YouTube's
  light and dark themes (system, light or dark, applied to the page too).
- **TV mode extras**: gamepad support, keyboard shortcuts for volume, speed and captions,
  4K unlock on smaller displays, low-memory mode, voice search.
- Snap (kde-neon-6) and Flatpak (KDE 6.11 runtime) packaging, built in CI.

### Changed
- Settings live in `~/.config/ktechpit/red.conf`; data in `~/.local/share/ktechpit/red/`.
  Red 9 settings are not migrated.

### Removed
- Red 9's app lock, licensing, rate-app prompts, intro slides, history tab, forced theater
  mode, custom scrollbars, "watch in mpv", and the remote filter lists.
