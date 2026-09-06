<h1 align="center">Red</h1>

<p align="center">
  <b>YouTube, in a real desktop app.</b><br>
  A living-room TV mode, ad and tracker blocking, SponsorBlock, media keys, a mini player and a
  real download manager — lightweight and open source, built with Qt&nbsp;6.
</p>

---

## Why Red

YouTube is great, but a browser tab is a poor home for it. Red gives it a real window with the
things a browser cannot do: it blocks ads before the player sees them, skips sponsor segments,
answers your media keys, shrinks into a mini player, becomes a TV, and downloads anything on
YouTube — video, audio, playlists, channels — with subtitles, thumbnails and chapters embedded.
It is a thin native shell, **no Electron**, so it starts fast and stays light.

## Features

**Two modes**
- **Desktop**: the full YouTube site, signed in, themed light / dark / system.
- **TV** (Ctrl+T): YouTube's smart-TV interface with keyboard, mouse and gamepad control,
  full screen by default.

**Blocking**
- Ads stripped from player and feed responses before YouTube renders them, plus a tiny
  network-level list for ad and analytics networks. Playback hosts are never touched.
- SponsorBlock: skip or mark eight segment categories, with progress-bar markers.
- Hide Shorts.

**Downloads (yt-dlp)**
- yt-dlp and its JavaScript runtime set themselves up on first use and keep themselves
  updated; ffmpeg comes from your system (the snap and Flatpak runtimes ship it).
- Quality presets, MP4 / MKV / WebM, audio-only in MP3 / M4A / Opus / FLAC / WAV, subtitles,
  embedded thumbnail, metadata and chapters, sponsor-segment removal, playlist and channel
  entry picking, an advanced per-stream table.
- A queue with pause / resume / retry, notifications with *Show in folder*, and your YouTube
  sign-in reused for age-restricted or rate-limited videos.

**Desktop integration**
- Media keys and desktop media controls (MPRIS), tray icon with playback controls,
  keeps the screen awake while playing, mini player (Ctrl+Shift+M), single instance with
  `red <youtube-link>` and `red --download <link>`.

## Build from source

Requires Qt **6.11** (the version shipped by the snap `kf6-core24` runtime and the Flathub KDE
runtime), CMake ≥ 3.21, and a C++20 compiler.

```sh
# Development build against the KDE Qt 6.11 snap SDK (works on any distro)
sudo snap install kde-qt6-core24-sdk kf6-core24
scripts/dev-build.sh --tests    # configure + build + run the test suite
scripts/dev-run.sh              # launch (scripts/dev-run.sh -- --tv for TV mode)

# …or with a system Qt >= 6.11
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Project layout

```
src/core      pure logic (settings, URL rules, block list, download queue) — Qt Core only, unit-tested
src/services  network/process services: engine provisioning, yt-dlp probing, SponsorBlock
src/web       WebEngine profile/page/view, the request interceptor, injected scripts, the JS↔C++ bridge
src/platform  OS backends behind interfaces (notifications, MPRIS, screen inhibit, file manager)
src/ui        QtWidgets: main window, rail, downloads panel, dialogs, tray
src/app       Application object, CLI, single instance
tests/        Qt Test suites (unit + offscreen smoke)
```

## License

[MIT](LICENSE) © Keshav Bhatt

<sub>YouTube is a trademark of Google LLC. Red is an independent client and is not affiliated
with, endorsed by, or sponsored by YouTube or Google.</sub>
