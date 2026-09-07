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
- **Permissions**: network, GPU and `/dev/input` (`--device=all`, gamepads), notifications,
  status notifier (tray), MPRIS bus name, screen saver, and the Videos / Music / Downloads
  folders; anything else goes through the file chooser portal.
- **CI**: `.github/workflows/flatpak.yml` builds x86_64 and aarch64 bundles on manual runs
  and attaches them to the GitHub Release on a version tag. Flathub builds from its own
  repository (`flathub/com.ktechpit.red`, same manifest with a `type: git` source pinned
  to the tag) once submitted.

### Submitting to Flathub

1. Tag a release (`v10.0.0`) and let the workflows run.
2. Copy `com.ktechpit.red.yaml` + `deno.yaml` into a `flathub/com.ktechpit.red` repository,
   replacing the `dir` source with `type: git`, `url`, `tag`, `commit`, and the usual
   `x-checker-data` block.
3. The metainfo needs screenshots (`<screenshots>`) and release notes for the Flathub
   quality guidelines; add them in `dist/linux/com.ktechpit.red.metainfo.xml` first.

## Validation without building

```
desktop-file-validate dist/linux/com.ktechpit.red.desktop
appstreamcli validate --pedantic dist/linux/com.ktechpit.red.metainfo.xml
snapcraft expand-extensions        # in the repo root, no build
DESTDIR=/tmp/red cmake --install build --prefix /usr && find /tmp/red -type f
```
