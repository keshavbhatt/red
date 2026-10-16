# PROGRESS

Newest first. One entry per working session.

## Milestone status

| Milestone | Status |
|---|---|
| M0 Skeleton | done |
| M1 Desktop mode | done (sign-in verified up to the Google form; mini player, MPRIS, theme verified live) |
| M2 Blocking | done (ad-heavy video plays clean; SponsorBlock skip observed live with toast) |
| M3 Downloads | done (engine self-provisions; video, audio-only and playlist downloads end to end) |
| M4 TV mode | done (Leanback loads, keyboard navigation verified over CDP; gamepad untested — no pad) |
| M5 Settings & polish | wip |
| M6 Packaging | recipes + workflows ready (snap, Flatpak, tests); first run needs the repo pushed |

## 2026-10-16: UX audit, mode switch

* UX audit of reaching the main features (19 items, P0 to P2); the first slice landed: a mode
  switch on the rail and in the tray (ADR-014), a *Back to YouTube* pill in TV mode, Settings,
  General *Open Red in* (default YouTube; `tv/startInTvMode` migrated), the mode in the window
  title. Verified with window grabs (rail in YouTube and Music, shortcuts sheet, Settings,
  TV entry). `tst_settings` covers the default, Last used and the migration.
* Second slice: the rail with labels (ADR-015, default on, toggle at the bottom and in Settings),
  F10 for the rail's menu with the rail hidden, Download / mini player labels that say what a
  click does or why it can't, and a scrolling rail in short windows (the mode switch from the
  first slice squeezed at 420 px). Verified with grabs in a separate `--profile uxtest`.
* Third slice: the mini player's close button restores the full window instead of quitting;
  Esc and a corner restore button (`ui::OverlayButton`, shared with the TV *Back to YouTube*
  pill) do the same. `RED_DEBUG_OPEN=mini|mini-close` verify it headless.
* Owner request: the mini player fits Shorts (9:16, `contain` lifted on the Short's slot) and works
  in Music mode around YouTube Music's own player page (ADR-016). Verified over CDP and grabs.
  Seen signed out: YouTube Music's `ytmusic-miniplayer` bar instead of `ytmusic-player-bar`
  (an experiment, it seems); next / previous still work for the owner.
* Fourth slice: the downloads panel floats over the page below 900 px instead of folding
  silently; a "Download added" / "Already in your downloads" hint where it cannot show; a
  duplicate no longer spends a free download; Settings confirms its silent actions inline.
  `RED_DEBUG_OPEN=downloads|announce` verify it.
* Open from the audit: first-run welcome, keyboard and
  screen-reader access to the rail and panel.

## 2026-09-13 — Session 3 (packaging, public repo)

* Red is proprietary (owner): every open-source/MIT claim removed; LICENSE is a proprietary
  notice; metainfo `LicenseRef-proprietary`; the Flathub route is the ultimate-media-downloader
  pattern (extract the published snap).
* Public repo github.com/keshavbhatt/red (metadata + issues, branch `master`) refreshed and
  pushed via `scripts/publish-public.sh`; source lives in the private github.com/keshavbhatt/new-red
  (created and pushed 2026-09-13; workflows tests/snap/flatpak ran for the first time).
* Original Red icon rebuilt as vector paths from Supercell Magic on the GNOME icon grid;
  composed screenshots + banner (dark theme, 4K nature films) for README and stores.
* Fixes on the way: YouTube PREF theme reconciled at every start; thumbnail keepsake no longer
  races yt-dlp; portal screen inhibit in sandboxes; minimal Flatpak permissions.

## 2026-09-07 — Session 2 (owner walkthrough)

* Owner tried the app live; each report became a commit: rail buttons turning into text (icons
  now on the actions), the settings divider, mini player gated on a playing video, audio-only
  as a first-class card in the download dialog (last kind remembered, "Download audio only…" in
  the context menu), automatic playlist detection with per-scope caches (the old checkbox lost
  the video URL and kept the error; YouTube mixes `RD…` are unviewable for yt-dlp and are now
  plain videos), subtitle toggles aligned, "Check for updates" feedback, internals renamed to
  "download engine" / "media converter".
* Exit criteria closed headlessly: a real SponsorBlock skip (37–52 s sponsor jumped, toast shown)
  and a playlist download end to end (`RED_DEBUG_AUTODOWNLOAD=audio-playlist:3-4` → two numbered
  Opus files in the playlist folder; selected count fixed via `playlist_autonumber`/`n_entries`).
* Still needs the owner: sign-in with a real account + signed-in download, gamepad on hardware.
* Polish sweep (headless, both themes): About, Shortcuts (redesigned: groups + key caps), tray
  menu (text only), all seven settings pages in light, the offline page, first run from an empty
  profile (engine sheet no longer squeezed when the progress bar appears). Download cards use
  the standard i.ytimg.com thumbnail. `RED_DEBUG_OPEN=about|shortcuts|tray` and popup grabs added.
* Sign-in verified by the owner; a signed-in (age-gated) download verified headlessly after
  fixing the cookie tracker (Qt 6.11 `loadAllCookies` is a no-op → the Cookies DB is read).
* Packaging (M6): Flatpak manifest on the ktechpit Flathub pattern (Qt WebEngine BaseApp +
  KDE 6.11, ffmpeg from the runtime, Deno bundled) with a workflow attaching bundles to
  releases; snap recipe validated (expand-extensions, install layout, desktop-file-validate,
  appstreamcli); tests workflow pins clang-format 22 and a session bus; screenshots, README
  install section, CHANGELOG, release notes. Nothing has run in CI yet: needs the push.
* Owner decision: the snap is built by GitHub Actions like whatsie, never locally (a local
  `snapcraft --use-lxd` attempt was stopped and its LXD project removed).

## 2026-09-06 — Session 1 (autopilot), continued

* Full app runs (`scripts/dev-run.sh`): rail + page + downloads panel; 26 tests green.
* Verified live over CDP and window grabs (desktop locked, so `RED_DEBUG_GRAB` was added):
  desktop home/watch pages, light/dark, downloads panel, download dialog (video + playlist),
  engine setup sheet (yt-dlp 2026.08.19 fetched and SHA-verified), a completed download with
  embedded thumbnail and a freedesktop notification, MPRIS metadata via `qdbus`, TV mode
  welcome → account selector via CDP key events, mini-player page state, settings pages.
* Findings that became code: YouTube's theme lives in the PREF cookie `f6` bits (ADR-009);
  TV mode's CSP needs the page nonce on injected `<style>` (LESSONS V2 → `__red.style()`);
  system ffmpeg breaks under the dev-run `LD_LIBRARY_PATH` → engine children get a clean env;
  a dialog opened before the full-screen main window is mapped gets closed on Wayland →
  startup commands are deferred 400 ms.
* Later the same session: Return YouTube Dislike shipped (D14 → KEEP); owner decision — ffmpeg
  is never downloaded (system ffmpeg, install hint, "Check again"); GitHub REST API replaced by
  release redirects (rate limit); Copy video link; live streams guarded; HLS duplicates hidden;
  Hide Shorts guide entry by icon type; parked-player media state fixed; snapcraft + CI drafted
  (M6 untested); `RED_NETWORK_TESTS=1` provisioning test green; 28 tests green under -Werror.
* Next session: gamepad on real hardware, sign-in past the Google form with the owner's account,
  owner walkthrough of the UI on a live desktop, snap build.

## 2026-09-06 — Session 1 (autopilot)

* Audited Red v9 (report in `reference/`), researched TV mode, read new-whatsie's standards,
  decisions and code; verified yt-dlp_linux 2026.08.19 + quickjs-ng 0.16.2 extract 4K formats.
* Wrote FEATURES, DESIGN, DECISIONS, LESSONS, ROADMAP; bootstrapped the CMake skeleton from
  whatsie's generic infrastructure (log sink, crash handler, single instance, notifier, …).
