# Changelog

All notable changes to Red. The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [Semantic Versioning](https://semver.org/).

## [10.3.0] - 2026-10-17

### Added
- A **mode switch** at the top of the rail: YouTube, YouTube Music and TV, with the current one
  lit. The tray menu has the same three, and Ctrl+1, Ctrl+2 and Ctrl+3 pick them directly.
- TV mode shows a **Back to YouTube** button when it starts and whenever the mouse moves.
- Settings, General: **Open Red in** picks the mode Red starts in: YouTube, YouTube Music,
  TV mode or the mode used last.
- The side rail shows **labels** next to its icons. *Hide labels* at the bottom of the rail (or
  Settings, General) brings back the narrow icons-only rail.
- F10 opens the rail's menu, also when the rail is hidden.
- F6 (and Shift+F6) moves the keyboard between the page, the side rail and the Downloads panel.
  On the rail the arrow keys move, Enter or Space presses and Esc returns to the page; Tab
  reaches the Downloads panel's buttons. A dashed ring shows where the keyboard is.
- A **welcome** on the first start shows the three modes, downloading, the mini player and
  blocking; *Show me around* in the rail's menu opens it again.
- The mini player has a button in its corner (it shows when the mouse moves) and Esc to go back
  to the full window.
- The mini player works in YouTube Music: a tall window with YouTube Music's own player (art,
  title, seek bar, controls and the queue).
- Shorts open the mini player in a tall window that fits the video.
- In a window narrower than 900 px the Downloads panel opens over the page instead of not at
  all; Esc closes it.
- "Download added" appears where the Downloads panel can't (full screen, the mini player, TV
  mode, a narrow window), and "Already in your downloads" when the same download is queued.
- Download cards have a right-click menu with every action (Open, Show in folder, Pause, Resume,
  Retry, Cancel, Copy link, Copy error, Remove), work from the keyboard (Enter, Delete, Space, the
  Menu key) and are read out by screen readers. A failed download shows its whole reason.
- Settings confirms Clear cache, Copy diagnostics, Reset site permissions and Reset all settings
  right under the buttons.
- **Middle-click scrolling**: Settings, General, *Scroll with the middle mouse button*. Middle-click
  the page and move the mouse to scroll; click again to stop. Off by default, since middle-click
  pastes on Linux; takes effect after a restart.

### Changed
- Red opens in YouTube unless set otherwise, instead of in whichever mode was open when it
  closed. "Start Red in TV mode" carries over as *Open Red in: TV mode*.
- The window title names the mode in YouTube Music and TV mode.
- The rail's Download button says what a click does: *Download this* on a video, playlist or
  channel, *Download from link* elsewhere. The mini player button says why it is unavailable.
- In a short window the rail scrolls instead of squeezing its buttons together.
- On-screen hints stay up long enough to read the longer ones.
- Colours follow YouTube's: red only for small marks such as progress bars, badges and the rail's
  Download button when there is something to download, YouTube's light (or dark) pill for main
  buttons, and YouTube blue for check boxes and selections.

### Fixed
- In YouTube Music, Next and Previous could show as unavailable, so the desktop's media controls,
  the tray and Bluetooth headsets could not skip tracks.
- Choices in Settings, General (the start page, the tray icon, download notifications) were
  forgotten after a restart. Earlier choices are picked up again.
- Closing the mini player's window returns to the full window instead of quitting Red.
- Download progress bars move smoothly instead of jumping every second or so.
- With *Advanced* picked and no stream chosen yet, the download dialog previewed an audio file
  name; it shows the video file now.
- The Blocking page of Settings no longer runs past the dialog's right edge; the SponsorBlock
  choices sit in one column.

## [10.2.0] - 2026-09-15

### Added
- Downloads are sorted into Videos, Music, Playlists and Channels folders; a switch in
  Settings, Downloads keeps the flat folder instead.
- The *Download* entry in YouTube Music's own menus (song rows, the queue, the player bar)
  downloads through Red instead of asking for Premium.

### Fixed
- The context menu's "Download audio only" entry has an icon in the same style as the others.

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
