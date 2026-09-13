# Architecture Decision Records

Short, numbered, append-only. Format: Context → Decision → Consequences.
ADRs inherited unchanged from new-whatsie are listed once in ADR-000.

---

## ADR-000 — Inherited from new-whatsie (2026-09-06)

The rewrite adopts, verbatim in spirit and mostly in code, these whatsie decisions:
Qt 6.11 minimum built against the KDE snap SDK (W-ADR-001); C++20/CMake only, warnings as
errors in CI (W-ADR-002); layered static libraries `core → web/platform → ui → app`
(W-ADR-003); no `.ui` files, PMF connects only (W-ADR-004); one typed `core::Settings`
facade (W-ADR-005); injected JS as resources on the **profile-level** script collection with
one `QWebChannel` bridge (W-ADR-006); user agent derived from the engine default (W-ADR-007);
Chromium sandbox on outside snap (W-ADR-008); freedesktop notifications over QDBus
(W-ADR-009); single instance via `QLocalServer` (W-ADR-010); no destructive self-healing
(W-ADR-011); tests and logging mandatory (W-ADR-012); GPU default on with crash-probe
fallback to SwiftShader (W-ADR-032); `FileManager1.ShowFolders` for "show in folder"
(W-ADR-027). Files carried over keep their whatsie header comments where the reasoning is
still the reasoning.

## ADR-001 — Product shape: one window, two modes (2026-09-06)

**Context.** Owner brief: Red works in two modes, desktop YouTube and a Leanback TV
mode. YouTube's Leanback UI needs a Cobalt user agent; the desktop UI needs a Chrome one.
The user agent is a per-profile property in Qt WebEngine, and the login session (cookies)
should be shared between modes.

**Decision.** One `QWebEngineProfile`, one `WebView`, one main window. A `web::Mode`
(`Desktop` / `Tv`) selects the URL, the profile `httpUserAgent` (what `navigator.userAgent`
reports), the per-request `User-Agent` header set by the interceptor (what YouTube's servers
see — the LESSONS V1 split: old Cobalt for the navigator, newer Cobalt on the wire for
`www.youtube.com`, plain `Red/x` elsewhere), the script bundle config (`mode` field), and the
window chrome (rail hidden, 16:9, fullscreen). Switching modes reloads the page. Only one mode
is active at a time, so the profile-wide UA never conflicts.

**Consequences.** Shared sign-in, one download pipeline, one settings surface. The cost is a
reload on switch (sub-second). Running both UIs side by side is out of scope.

## ADR-002 — Identity `com.ktechpit.red`, settings under `ktechpit/red` (2026-09-06)

The v9 snap was `red-app` with settings in `org.keshavnrj.ubuntu/Red`. New identity:
`applicationName=red`, `organizationName=ktechpit`, desktop/AppStream id `com.ktechpit.red`.
No migration from v9 (its settings were licensing + a base64 password). Version starts at
**10.0.0**, above v9.

## ADR-003 — Ad blocking in three cheap layers, curated lists, no filter engine (2026-09-06)

**Context.** Red v9's interceptor scanned ~630 substrings per request, touched `QSettings`
twice per request and wrote a counter per block — the reported lag. It also blocked specific
`googlevideo.com` edge hosts (region-specific, broke playback). YouTube today detects
network-level ad blocking and shows an enforcement dialog; the reliable approach (uBlock
Origin's) strips ads from the InnerTube responses before the player sees them.

**Decision.**
1. `core::BlockList`: a `QSet` of blocked host suffixes (third-party ad/analytics domains)
   plus a short list of `youtube.com` path prefixes (`/pagead/`, `/api/stats/ads`,
   `/ptracking`, `/youtubei/v1/log_event` [trackers toggle]…). `matches(url)` walks the
   host's label boundaries (≤ 6 hash lookups) and checks path prefixes only for
   YouTube hosts. Pure, unit-tested, never touches `googlevideo.com`.
2. `web::RequestInterceptor` (profile-level, runs on the IO thread) holds a `std::shared_ptr`
   to an immutable `BlockList` snapshot swapped atomically on settings change; blocked
   requests bump a `std::atomic<quint64>` — no signals, no settings, no logging per request
   (a debug-category summary every 100 blocks).
3. `adblock.js` (DocumentCreation, MainWorld) hooks `JSON.parse`, `Response.prototype.json`/
   `text` for `/youtubei/v1/(player|next|browse|search|guide|reel…)`, `XMLHttpRequest`, and
   defines setters on `ytInitialPlayerResponse` / `ytInitialData` to strip `adPlacements`,
   `adSlots`, `playerAds`, `adBreakHeartbeatParams` and feed/shelf ad renderers.
4. `adblock.css` hides the residual containers and the enforcement dialog; a
   player-scoped `MutationObserver` clicks skip buttons and fast-forwards `.ad-showing`.

**Consequences.** Zero measurable per-request cost, no lists to update from the network, and
stealthier than host blocking alone. YouTube's `/youtubei` shapes change; the JS side is
guarded and reports failures over the bridge.

## ADR-004 — yt-dlp engine: standalone binary + QuickJS + ffmpeg, provisioned on demand (2026-09-06)

**Context.** v9 ran `python3 core` (the zipimport build) and never checked for Python or
ffmpeg — broken under snap confinement. Since 2025 yt-dlp needs an external JavaScript
runtime for YouTube's signature challenges (EJS). Verified 2026-09-06: `yt-dlp_linux`
2026.08.19 + `qjs` (quickjs-ng 0.16.2, 2.5 MB) extracts all 53 formats of a 4K video with no
warnings; without a runtime high-quality formats are missing.

**Decision.** `core::EngineManager` provisions into `<AppData>/engine/`:
* `yt-dlp` — the standalone `yt-dlp_linux[_aarch64]` release asset, SHA-256 verified against
  the release's `SHA2-256SUMS`; version = release tag (a date, compared as `QDate`).
* `qjs` — quickjs-ng release asset for the CPU, unless a usable `deno`/`node ≥ 22`/`qjs` is
  found on `PATH` or bundled (`$SNAP/usr/bin`). Passed via `--js-runtimes`.
* `ffmpeg` — **never downloaded** (owner revision, 2026-09-06): the system's `ffmpeg` on
  `PATH` or in `$SNAP/usr/bin`; the snap and Flatpak runtimes ship it and distro packages
  exist everywhere. When it is missing the engine reports `ffmpegMissing` with a distro-aware
  install hint (`core::ffmpegInstallHint()`) and "Check again" re-detects. Prebuilt ffmpeg
  binaries were also an arm64/amd64 liability. Passed via `--ffmpeg-location`.
All fetches are async `QNetworkAccessManager` downloads with progress signals; nothing blocks
the GUI thread. A "Use system yt-dlp" setting bypasses provisioning for the binary.

**Revision (2026-09-06, evening).** Release lookups no longer touch the GitHub REST API (60
requests/hour per IP, a first-run failure waiting to happen behind NAT): `releases/latest/
download/<asset>` is fetched with a manual redirect policy and the tag is read from the first
hop (`releases/download/<tag>/<asset>`); the update check does the same with `releases/latest`.
The `SHA2-256SUMS` file is fetched from the tagged URL.

**Consequences.** Works out of the box on any Linux with ffmpeg installed (no Python); ~45 MB
first-time download. The opt-in network test (`RED_NETWORK_TESTS=1 ctest -R engine`) exercises
the real provisioning with an empty PATH.

**Snap and Flatpak (revised 2026-09-07).** Both bundle Deno (v2.7.7, per architecture) as the engine's JavaScript runtime; QuickJS is only downloaded for native builds without a runtime on PATH.  Nothing ffmpeg-related is staged: the kf6-core24 runtime ships only
the libav* libraries (for Qt Multimedia), not the `ffmpeg` executable yt-dlp runs, so the snap
plugs the `ffmpeg-2404` content snap (`$SNAP/ffmpeg-platform`, default-provider, on PATH and
LD_LIBRARY_PATH in the app environment) and the engine also searches
`$SNAP/ffmpeg-platform/usr/bin`. Flatpak: the freedesktop runtime has no ffmpeg CLI either →
`org.freedesktop.Platform.ffmpeg-full` extension, later.

## ADR-005 — yt-dlp is driven through JSON in and JSON out (2026-09-06)

**Context.** v9 parsed the `-F` human table and progress lines with regexes and broke on every
yt-dlp formatting change.

**Decision.** Probing uses `-J` (`--dump-single-json`, `--flat-playlist` for multi-entry
URLs) parsed by `core::media_info` into typed structs. Downloads run with `--newline
--progress-template "download:RED:%(progress)j" --progress-template "postprocess:REDPP:%(progress)j"
--print after_move:filepath` so `core::ytdlp_output::parseLine` sees one JSON object per line.
Option → argv mapping (`core::download_options::toArguments`) is pure and unit-tested; the
format selector is built from the preset (`bv*[height<=1080]+ba/b[height<=1080]`) or from an
explicit `video+audio` pick. `--no-update --no-colors --ignore-config` always.

## ADR-006 — Sign-in cookies for downloads come from the app's own session (owner, 2026-09-06)

**Context.** YouTube now demands a signed-in session after a few anonymous extractions
("Sign in to confirm you're not a bot"). The owner chose to reuse the app's session over asking
for a cookies file. yt-dlp's wiki warns that account cookies rotate when the browser is open;
the app *is* the browser, so the cookies are re-exported before every run.

**Decision.** `web::CookieExporter` mirrors the profile's cookie store (`loadAllCookies` +
`cookieAdded`/`cookieRemoved`) for the `youtube.com` / `google.com` / `googlevideo.com`
domains. Before each yt-dlp run it writes a Netscape cookie file to a private temp file
(`QTemporaryFile`, `0600`) passed with `--cookies`; the file is removed when the process ends.
Setting "Use my YouTube sign-in for downloads" (default on). Nothing is ever written to the
config directory.

**Revised 2026-09-07.** `QWebEngineCookieStore::loadAllCookies()` delivers nothing in Qt 6.11:
`CookieMonsterDelegateQt::getAllCookies()` calls `GetAllCookies` with an empty callback, so only
cookies that change while the app runs ever reach `cookieAdded`. A persisted sign-in therefore
never reached yt-dlp. The exporter now reads the profile's `Cookies` SQLite database itself
(`services::readChromiumCookieDatabase`, a private copy of the locked file; QtWebEngine stores
values in clear) as the base, keeps live store changes on top, and re-reads right before every
export because Chromium flushes lazily. Verified: an age-gated video (`Tq92D6wQ1mg`) probes and
downloads with the owner's session.

**Thumbnails (2026-09-07).** Download cards must not depend on YouTube still serving a picture
(private or removed videos, `i9.ytimg.com` maxres frames that 404). Three layers in
`ui::ThumbnailCache`: memory → the 64 MB HTTP disk cache → a *keepsake* per job,
`<AppData>/thumbnails/<jobId>.jpg` at 480 px wide (~15–40 KB), written when the job is queued
and deleted with it. For single videos yt-dlp itself writes the thumbnail there
(`--write-thumbnail --convert-thumbnails jpg -o thumbnail:<dir>/<id>.%(ext)s`): it fetches with
the session, which is the only way to get a private video's picture. A frame that is not served
falls back to the standard `i.ytimg.com/vi/<id>/mqdefault.jpg`.

## ADR-007 — Media state, MPRIS and SponsorBlock flow through the bridge (2026-09-06)

**Context.** Media keys, tray play/pause, screen inhibit and SponsorBlock all need to know
what is playing; SponsorBlock/DeArrow HTTP calls from inside the page hit YouTube's
`connect-src` CSP (an Electron app could rewrite the response headers; Qt WebEngine cannot).

**Decision.** `media-session.js` observes the `<video>` element and `navigator.mediaSession`
metadata and reports `{state, title, artist, artwork, duration, position, videoId}` through
`Bridge::mediaStateChanged` (throttled: events + 1 s while playing). C++ consumers:
`platform::MprisService` (QDBus adaptors), `TrayController`, `ScreenInhibitor`, and
`core::SponsorBlockClient` which fetches segments (k-anonymity `sha256(videoId)[0:4]`) and
pushes them back via `Bridge::sponsorSegments(videoId, json)`. Commands (play/pause/next/seek)
travel C++ → JS as bridge signals; the script acts on the player.

## ADR-008 — Chrome UA for sign-in, no Firefox spoof by default (2026-09-06)

v9 spoofed Firefox 72 to get past Google's "browser may not be secure" check. Whatsie showed
that the engine's real Chrome UA minus the `QtWebEngine/` token is the honest, working option.
Red uses it in desktop mode. A "Sign-in user agent: Firefox" escape hatch exists in Advanced
for the day Google tightens the check again.

**Revised 2026-09-07.** Removing the QtWebEngine token was not enough: the owner got "Couldn't
sign you in — this browser or app may not be secure" after entering the address. Three changes,
verified headlessly against the real sign-in flow (a nonexistent address now gets "Couldn't find
this account" instead of the rejection, on a fresh profile and on the owner's):
1. The Firefox identity is a *per-request header* on `accounts.google.com` / `accounts.youtube.com`
   only (`RequestInterceptor::setSignInUserAgent`), default on; YouTube keeps the Chrome UA and
   `navigator.userAgent` is untouched. The old whole-profile Firefox switch is gone.
2. The injected bundle bails out on non-YouTube hosts (bootstrap.js): Google's bot check inspects
   patched natives such as `JSON.parse`, which Red's hooks replace.
3. A rejection sticks to the session (every later entry lands on `/v3/signin/rejected`); the view
   clears Google's cookies and re-enters the flow once (`WebView::recoverFromSignInRejection`).


## ADR-009 — YouTube's theme is set through the PREF cookie, not the DOM (2026-09-06)

**Context.** theme-control.js toggled `html[dark]`, which removed the attribute fine, but the
page stayed dark: YouTube's current styling ("color-version v2") follows
`prefers-color-scheme` while the account/device preference is "device theme", and Qt WebEngine's
`prefers-color-scheme` follows the platform, not `QStyleHints::setColorScheme` (whatsie
ADR-026 all over again). Verified over CDP: writing `PREF=f6=80000` and reloading renders
light; YouTube rewrote it to `f6=40080000`, i.e. `f6` is a bit field with `0x80000` = light,
`0x400` = dark, neither = device.

**Decision.** `core::youTubePrefForTheme()` rewrites only those two bits of the existing PREF
value (other flags — autoplay, playback quality, `tz` — are preserved); `WebProfile::
applyThemeCookie()` writes it through the profile's cookie store whenever the theme setting
changes or YouTube rewrites PREF, but never before the store has loaded (a bare PREF would
wipe the other flags). theme-control.js stays for the live toggle on the current page.

**Consequences.** Explicit Light/Dark render correctly on the next load and instantly on the
current page; System follows the OS as before. Unit-tested bit handling.

## ADR-010 — Headless verification hooks (2026-09-06)

The owner's desktop was locked for the whole first session, so two developer aids exist and are
documented rather than hidden: `RED_DEBUG_GRAB=<png>[,<seconds>]` saves `QWidget::grab()` of the
window and every visible dialog on a timer, and `RED_DEBUG_AUTODOWNLOAD=1` accepts the download
dialog with its defaults once the probe finishes; `RED_DEBUG_SETTINGS_PAGE=<n>` opens Settings on
a page. Together with `scripts/cdp-eval.mjs` (remote debugging on port 9422) they let a headless
session exercise the real UI end to end. They read environment variables only, cost nothing when
unset, and ship (they are harmless and useful for bug reports).

## ADR-011 — Accounts and licensing (2026-09-13)

Red uses the shared ktechpit **AccountAndLicense** module (copied into
`src/modules/AccountAndLicense`, patched only for `QT_NO_KEYWORDS` / the string builder and
an `ensureSerial(legacySerial)` argument). App code **RED**, checkout slug `red`, USS backend,
**10-day evaluation**. `services::LicenseService` wraps it; `ui::AccountDialog` (rail menu,
`RED_DEBUG_OPEN=account`) and the gate prompt (`MainWindow::promptUpgrade`) are the UI.
Semantics: `status.active` = paid (Pro); `hasPaidAccess` = access (paid *or* evaluation).

**Migration**: a Red 9 account ID is read from `<home>/.config/org.keshavnrj.ubuntu/Red.conf`
(`accountId`) or `<home>/Downloads/.red.id`, for the process home, `SNAP_REAL_HOME`, and
`~/snap/red-app/{current,common}`; adopted only when nothing is stored yet, and mirrored back
to `~/Downloads/.red.id`. Verified against the server with the owner's Red 9 id.

**Gated**: downloads, all of them (owner decision) — one choke point,
`DownloadsHub::requestDownload`, plus resume/retry. Nothing else is gated.

**Never in release builds**: the `RED_DEBUG_LICENSE` override exists under `QT_DEBUG` only
(a Debug tree lives in `build-debug/`); an env override in a shipped binary would be a bypass.

## ADR-012 — "What's new" once per version, from the bundled CHANGELOG (2026-09-13)

`CHANGELOG.md` is compiled into `red_ui` (`:/text/CHANGELOG.md`); `core::changelogSection`
extracts the running version's section and `ui::WhatsNewDialog` renders it (Markdown in a
QTextBrowser on a card, "Full changelog" → the public repo, "Got it"). Shown once per version the
user has not seen (`window/whatsNewSeenVersion`), the first Red 10 start included — Red 9 settings
do not carry over, so for upgraders the first run *is* the upgrade. Marked seen when shown, not
on close (a crash cannot loop it); `resetToDefaults()` re-shows it once (accepted).
Scheduling: after the modal GPU notices return, plus 3 s, so it lands past the Wayland→xcb
relaunch probe. `RED_DEBUG_OPEN=whatsnew` forces it. Release checklist: replace `— unreleased`
in the heading with the date before tagging.

**Rule (owner, CLAUDE.md):** accounts and licensing never appear in user-facing release notes;
`tst_changelog` fails if the changelog mentions them, and checks the project version has notes.
