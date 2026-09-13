# Packaging

Red is packaged for Linux as a **snap** and a **Flatpak**. Both are built in CI, never on
developer machines (owner decision, 2026-09-07: the KDE SDK images are multi-GB).

## Snap

`snap/snapcraft.yaml` builds against the **kde-neon-6** extension (Qt 6.11 from the
`kf6-core24` content snap — the same runtime `scripts/dev-run.sh` targets).

- **App id** `com.ktechpit.red`; desktop file, AppStream metainfo and hicolor icons are
  installed by CMake from `dist/linux/` and `src/resources/icons/hicolor/`; `adopt-info`
  takes summary and description from the metainfo, and the version is set from the CMake
  project (one source of truth), as in the ultimate-media-downloader snap.
- **ffmpeg** comes from the `ffmpeg-2404` content snap (plug with default-provider, on
  PATH and LD_LIBRARY_PATH in the app environment); nothing is staged or downloaded
  (ADR-004). The kf6 runtime ships only the libav* libraries, not the program.
- **Deno** is bundled by the `deno` part (per-architecture release zip, checksum-checked,
  same release as the Flatpak) so the download engine has its JavaScript runtime without
  fetching QuickJS; only yt-dlp itself is fetched at first use.
- **Plugs** each have a reason: `joystick` (gamepads in TV mode), `screen-inhibit-control`,
  `audio-record` (voice search), `removable-media` (downloads to external drives),
  `browser-support` and the network plugs (WebEngine).
- **CI**: `.github/workflows/snap.yml` builds amd64 and arm64 on every push to `main`;
  with the repo variable `PUBLISH_TO_STORE=true` and the `SNAPCRAFT_STORE_CREDENTIALS`
  secret it releases to **edge**. Nothing is stored as a workflow artifact.

## Flatpak

`packaging/flatpak/com.ktechpit.red.yaml` follows the other ktechpit apps on Flathub:
`io.qt.qtwebengine.BaseApp` 6.11 on `org.kde.Platform` 6.11.

- **ffmpeg** is in the KDE runtime; **Deno** is bundled (`deno.yaml`) so the download
  engine has its JavaScript runtime without fetching QuickJS. yt-dlp itself is still
  fetched into the app's data directory at first use and kept up to date (ADR-004).
- **Permissions** are the minimum: network, the sockets, GPU, `/dev/input` (gamepads in TV
  mode), the status notifier for the tray, the MPRIS bus name, and the Downloads folder for the
  default download location. Notifications, screen inhibit, opening files and folders and
  choosing another download folder all go through portals inside the sandbox
  (`FLATPAK_ID` / `SNAP` select the portal backends).
- **CI**: `.github/workflows/flatpak.yml` builds x86_64 and aarch64 bundles on manual runs
  and attaches them to the GitHub Release on a version tag.

### Submitting to Flathub

Red is closed source, so the Flathub manifest cannot build from a git tag. Follow the
ultimate-media-downloader pattern: a `simple` module that downloads the published snap for
each architecture (`https://api.snapcraft.io/api/v1/snaps/download/<id>_<rev>.snap`, sha256
pinned), `unsquashfs` it, and installs the binary, icons, desktop file and metainfo from
`squashfs-root/usr/`. `deno.yaml` stays as is; ffmpeg comes from the runtime. The metainfo
declares `LicenseRef-proprietary`; screenshots must be served from a public URL.

## Validation without building

```
desktop-file-validate dist/linux/com.ktechpit.red.desktop
appstreamcli validate --pedantic dist/linux/com.ktechpit.red.metainfo.xml
snapcraft expand-extensions        # in the repo root, no build
DESTDIR=/tmp/red cmake --install build --prefix /usr && find /tmp/red -type f
```
