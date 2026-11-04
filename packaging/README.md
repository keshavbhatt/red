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
- **CI** (ADR-021): `.github/workflows/snap.yml` builds amd64 and arm64. A push to `master`
  publishes to **edge** as `X.Y.Z+git<hash>`; a `vX.Y.Z` tag publishes `X.Y.Z` to **stable**
  after checking it against `project(VERSION)`. Publishing needs the repo variable
  `PUBLISH_TO_STORE=true` and the `SNAPCRAFT_STORE_CREDENTIALS` secret. A newer push cancels
  an older branch build, never a tag's. Nothing is stored as a workflow artifact.

### Store listing text

The store listing (title, summary, description) is edited by hand at
https://snapcraft.io/red-app/listing; uploads never change it. `snap/snapcraft.yaml` carries
the same text so the snap's own metadata matches, but the listing page is the source of truth.
Limits: title 40 characters, summary 79, description 4096; syntax is `**bold**`, `_italics_`,
`* list`, backtick code and bare URLs.

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
- No CI build: like whatsie, only the snap is built in CI. The manifest is the base for the
  Flathub repository.

### Submitting to Flathub

The Flathub manifest repackages the published snap (it predates the public source; building
from a git tag is a possible follow-up). The submission lives
in the fork clone at `/home/commander/DCode/other/flathub-repositories/flathub`, branch
`add-red` (from upstream `new-pr`): `com.ktechpit.red.yaml` plus the `shared-modules`
submodule, following ultimate-media-downloader. A `simple` module downloads the published snap
for each architecture (`https://api.snapcraft.io/api/v1/snaps/download/<id>_<rev>.snap`,
sha256 pinned), `unsquashfs` it, and installs the binary, Deno, icons, desktop file and
metainfo from `squashfs-root/usr/`; ffmpeg comes from the runtime. The snap URLs and checksums
are filled in by hand (the store info endpoint lists them:
`curl -H 'Snap-Device-Series: 16' 'https://api.snapcraft.io/v2/snaps/info/red-app?fields=download,revision'`).
Lint without building: `flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest com.ktechpit.red.yaml`.
The PR targets `flathub/flathub` branch `new-pr`, title `Add com.ktechpit.red`, and needs a
short video of the Flatpak build attached. Flathub builds it in their CI; never build locally. The metainfo's licence is
`GPL-3.0-or-later` only: a `LicenseRef-proprietary=<URL>` part makes Flathub show "Special
License"; the module's own licence is in LICENSING.md and REUSE.toml.

## Before tagging a release

- `CHANGELOG.md`: replace the unreleased marker in the `## [x.y.z]` heading with
  ` - YYYY-MM-DD` (`release.yml` refuses an undated section); no
  accounts/licensing text (CLAUDE.md; `tst_changelog` enforces it). The dialog shows this section.
- `dist/linux/com.ktechpit.red.metainfo.xml`: matching `<release version date>` notes.
- `CMakeLists.txt`: `project(VERSION)` bumped.
- Check the edge build of that commit, then `git tag vX.Y.Z && git push origin vX.Y.Z`: the
  snap goes to stable and `release.yml` creates the GitHub release from the changelog section.

## Validation without building

```
desktop-file-validate dist/linux/com.ktechpit.red.desktop
appstreamcli validate --pedantic dist/linux/com.ktechpit.red.metainfo.xml
snapcraft expand-extensions        # in the repo root, no build
DESTDIR=/tmp/red cmake --install build --prefix /usr && find /tmp/red -type f
```
