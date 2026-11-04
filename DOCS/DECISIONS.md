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
window, every visible dialog and any open popup (menus, combo lists) on a timer, and
`RED_DEBUG_AUTODOWNLOAD=video|audio|advanced[-playlist][:from-to]` fills the download dialog once
the probe finishes and accepts it (`advanced`, unbounded playlists and `video-subtitles`, which
drops the subtitle picker open, stay open for a grab); `RED_DEBUG_SETTINGS_PAGE=<n>` opens
Settings on a page and `RED_DEBUG_OPEN=<dialog>` opens one of the app's dialogs;
`RED_DEBUG_SWIPE=<seconds>:<dx>:<steps>[:<hold ms>][;…]` plays touchpad swipes into the page
(ADR-022). Together with `scripts/cdp-eval.mjs` (remote debugging on port 9422) they let a headless
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

**Gated**: downloads, but not outright (owner decision, 2026-09-13): the free tier gets
**5 downloads a day** (`LicenseService::kFreeDownloadsPerDay`), paid and evaluation are
unlimited. A playlist counts its selected entries. `core::DailyAllowance` (day + count, in
`licensing/freeDownloads*` of the settings file, kept across *Reset all settings*) does the
arithmetic; `LicenseService::canDownload` / `spendDownloads` are the choke points, called from
`DownloadsHub::requestDownload` and the download dialog's admission check (`setAdmission`, so
an oversized playlist keeps the dialog open to pick fewer entries). Resume and retry are never
counted again. `MainWindow::promptDownloadLimit` explains and offers the plans. Nothing else
is gated. `ui::PlansDialog` (account sheet → *Compare plans*, the prompts' *View plans*,
`RED_DEBUG_OPEN=plans`) lists Free / Evaluation / Pro side by side so people see how much is
free and that Pro is what funds the upkeep (owner's framing, 2026-09-13).

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

## ADR-013: a third mode, YouTube Music (2026-09-14)

**Context.** YouTube Music links open through `target=_blank`, so they landed in a bare
`PopupWindow` where nothing of Red worked: no rail, no downloads, and `media-session.js`
reported nothing on any YouTube Music page that is not `/watch` because of its desktop-only
"parked" rule. YouTube Music is the same InnerTube backend, the same cookies and the same html5
player, so it fits the existing plumbing; only the chrome and a few page-side selectors differ.
Owner decision: a full mode, not a pop-up fix.

**Decision.** `core::AppMode::Music` next to Desktop and TV (ADR-001 extended, not replaced):
same profile and user agent as Desktop, script set = the shared scripts only (no hide-shorts,
theme-control, mini-player, dislikes), `mode: "music"` in the script config with
`red.isMusic()`. `WebView` maps videos and playlists to `music.youtube.com` URLs in Music mode
and keeps a separate `lastMusicUrl`. A new-window request for the music host
(`QWebEnginePage::newWindowRequested`, which carries the URL) switches the main view to Music
instead of opening a pop-up; a pop-up that navigates there hands the URL back and closes.
`media-session.js` never parks in Music, drives next/previous through `ytmusic-player-bar`, and
reports `album` and a site-specific `url` (MPRIS `xesam:album`, `xesam:url`). Downloads
preselect *Audio only* through `DownloadDialog::preselectKind`, which leaves the remembered
default alone unless the user picks something else; the rail's Download button follows the
playing track on every page (media-state fallback, as in TV). Mini player disabled, screen
inhibit off, page background dark. `--tv --music` is a parse error. One `MainWindow::setMode`
slot; the two checkable actions connect on `triggered` and are re-synced under a signal blocker.

**Consequences.** YouTube Music is dark-only, so the light theme stops at the chrome. Album
pages are not downloadable yet (M7). SponsorBlock segments are still fetched only on `/watch`
and markers are not drawn on YouTube Music's bar (M8). `yt-navigate-finish` does not fire on
YouTube Music, so the media reporter watches the URL from its tick.

## ADR-014: a mode switch and an explicit start mode (2026-10-16)

**Context.** TV and Music were two checkable rail buttons; Desktop was "neither checked", so the
way back was to click the lit button again or know Ctrl+T / Ctrl+M. TV mode hides the rail, so
the only visible instruction was a 3.5 s hint. `window/lastMode` was also the launch mode, so
closing Red in TV mode reopened it full screen with no chrome. UX audit 2026-10-16, items 1, 2,
12, 13.

**Decision.** Three checkable actions in one exclusive `QActionGroup` (`youtubeMode`,
`musicMode`, `tvMode`, Ctrl+1/2/3) drive an outlined mode switch at the top of the rail and
radio items in the tray; picking the active mode does nothing. Ctrl+T and Ctrl+M stay as
on/off toggles (window `QShortcut`s), because the hints, the TV controls card and the guide
teach them. TV mode gets `ui::TvExitButton`, a *Back to YouTube* pill shown on entry and on
mouse movement (application event filter, active only in TV mode). `general/startMode`
(YouTube, YouTube Music, TV, Last used; default YouTube) picks the launch mode through
`Settings::startupMode()`; `tv/startInTvMode` seeds it once (true → TV) and is removed on the
first explicit choice. `WebProfile` writes the resolved launch mode back to `lastMode`, so
"Last used" never resurrects a mode from an older session. The window title carries the mode
(`Red Music`, `Red TV`).

**Consequences.** Upgraders who relied on reopening in their last mode now open in YouTube
until they pick "The mode I used last". Leanback's own "Exit" still returns to YouTube (or
quits after `--tv`), worded by YouTube.

## ADR-015: a rail with labels (2026-10-16)

**Context.** The rail was 52 px of icons whose only labels were hover tooltips: slow to learn,
useless on touch, and Download / Downloads / Mini player / the modes look alike. Hiding the rail
stranded the More menu (Online guide, Account, Open log folder), while Settings claimed
everything stayed reachable. The Download button's idle tooltip ("Open a video…") did not match
its click (the paste-a-link sheet). UX audit 2026-10-16, items 3, 6, 7, 17.

**Decision.** `ui::RailButton` (a `QToolButton`) draws either the square icon button or, expanded,
the style sheet background plus the icon at its collapsed x position and a label beside it, with
an optional muted second line. Sizes are set in code; the style sheet no longer pins 40 px.
`window/railLabels` (default on, so new users see the names) is toggled by *Hide labels* /
*Show labels* at the bottom of the rail and in Settings, General; the rail only emits
`expandRequested`, `MainWindow` stores it. Labels: the action text without "…"; Download is
*Download this* or *Download from link* by page; Reload becomes *Stop* while loading; the mini
player shows why it is off. F10 (`Actions::menu`) opens the More menu from the rail, or as a
pop-up at the window corner when the rail is hidden. The rail's content sits in a scroll area
(scroll bars off, wheel scrolls) so a short window clips instead of overlapping buttons.

**Consequences.** The expanded rail takes 148 px more from the page; the downloads panel still
folds below 900 px of window width (audit item 5). The labels toggle adds one row, so on short
windows more of the rail sits below the fold.

## ADR-016: the mini player fits Shorts and YouTube Music (2026-10-16)

**Context.** The mini player was a 16:9 window around `#movie_player`. A Short (9:16, player
`#shorts-player`) ended up letterboxed in a wide window or not filled at all, and Music mode had
no mini player (ADR-013: "Mini player disabled"). YouTube Music has a responsive player page:
at about 360×640 it is a phone-style player with the art or video, title, seek bar, controls and
the Up next / Lyrics tabs. Owner request, 2026-10-16.

**Decision.** `core::mini_player_policy` maps the content to a shape (Wide, Shorts, Music) and a
shape plus the size setting to a window size; the window re-fits on navigation while the mini
player is on, and a mode switch turns the mini player off. `mini-player.js` marks Shorts pages
with `html.red-shorts`: the watch-page rules are scoped to `:not(.red-shorts)` (the parked
`#movie_player` would otherwise cover the Short), and `#shorts-player` is fixed to the window
with the action column, arrows and title overlay hidden. Each Short's slot
(`.reel-video-in-sequence-new`) has `contain: size layout`, which makes it the containing block
of fixed descendants, so the mini player lifts it. Music mode loads `music/mini-player.js`: it
opens the player page by clicking the visible `ytmusic-miniplayer` bar's title (the bar exists
twice; one copy is parked at zero size), retries while the layout reflows, hides the collapse
chevron, and closes the page again on exit if it opened it. The mini player is offered in Music
whenever something plays. The restore button sits top left in Music (the chevron's place; the
menu is top right). Window minimum in the mini player: 200×180.

**Consequences.** YouTube Music's DOM moved from `ytmusic-player-bar` to `ytmusic-miniplayer`
(`is-wiz-miniplayer-enabled`) in a signed-out profile, apparently an experiment: next / previous
still work for the owner. If it rolls out, `media-session.js` must read Next / Previous from
`ytmusic-miniplayer` instead of `ytmusic-player-bar`.

## ADR-017: YouTube's colour roles instead of one red accent (2026-10-16)

**Context.** Every emphasis in Red's chrome was `#FF0000`: primary buttons, the rail's Download
pill, ticked check boxes, radios, selection, progress, badges. The owner found it too bright and
asked to match YouTube. Sampled from the live page (dark): the logo, scrubber and progress start
are `#FF0033` (the progress bar fades to `#FF2791`); Subscribe is `#F1F1F1` with `#0F0F0F` text;
Sign in and other calls to action are `#3EA6FF` (light: `#065FD4`). YouTube's red is as bright as
Red's was; YouTube keeps it calm by using it only for small marks.

**Decision.** The `accent` token is replaced by YouTube's roles: `brand` (`#FF0033`: progress,
the downloads badge, the active mode glyph, dialog badges, the error page's mark), `primary`
(the inverted pill, `#F1F1F1` / `#0F0F0F`: primary buttons, the rail's Download pill, the error
page's button) and `control` (YouTube blue: check boxes, radios, text and list selection, the
palette's Highlight, the picked kind card). Owner choice among three options (also offered: a
deeper `#CC0000` red everywhere, or `#FF0033` in place of `#FF0000`).

**Consequences.** Red's chrome reads like YouTube's: mostly neutral, with red where YouTube puts
it. The brand still shows in the logo, the mode switch and every progress bar.

## ADR-018: a welcome for new installs (2026-10-16)

**Context.** A new user's first screen was the release notes of the running version (ADR-012):
nothing told them about the rail, the three modes, downloading or the mini player. UX audit
item 8.

**Decision.** `Settings::isFirstRun()` is true when neither the welcome (`window/welcomeSeen`)
nor any release notes (`window/whatsNewSeenVersion`) were ever shown: a new install, or a Red 9
upgrade (its settings do not carry over, ADR-012). Then the startup slot of What's new shows
`ui::WelcomeDialog` instead and marks this version's notes seen; Red 10 upgraders keep getting
What's new. The welcome is four rows (modes, downloads, mini player, blocking) naming the
shortcuts, with *Keyboard shortcuts* and *Get started*; `Actions::welcome` (*Show me around*, More
menu) reopens it. Marked seen when shown. `RED_DEBUG_OPEN=welcome`. The transient hint
(`web::FullScreenHint`) now stays for a reading time (2 s + 60 ms per character, 3.5 to 8 s).

**Consequences.** Red 9 upgraders see the welcome rather than the 10.x notes, which suits them:
Red 10 is a new app to them. `resetToDefaults()` shows the welcome again once.

## ADR-019: Settings, General under app/, not general/ (2026-10-17)

**Context.** While adding middle-click scrolling (public issue #17) the setting did not reach the
page after a restart. QSettings' INI format writes a group named `general` as `[%General]` and
reads that section back as `General/`, so every `general/` key (start page, start mode,
notifications, tray icon) was written but never found again: the choices reset on each start
since 10.0. The existing round-trip test passed because QSettings caches a file per process.

**Decision.** The keys move to `app/`. `Settings` migrates `General/` and `general/` values to
`app/` once at construction (an `app/` value wins). `tst_settings` now reads hand-written files
and checks the written text, so the in-process cache cannot hide a mismatch again.

**Consequences.** Users get their old General choices back with 10.3.0. Middle-click scrolling
is Chromium's own `MiddleClickAutoscroll` Blink feature, passed at start (restart to change);
`mergeChromiumFlags` now merges Blink feature lists with a user's own.


## ADR-020: open source, two licences by path (2026-11-03)

**Context.** Red 10 was proprietary (2026-09-13): the source lived in the private
github.com/keshavbhatt/new-red and the public github.com/keshavbhatt/red held only the README,
guide, changelog and screenshots. The owner decided to open the source the way Playlist
Downloader did (its ADR-008).

**Decision.** The code goes public at github.com/keshavbhatt/red on `master`, the branch the
shipped metainfo, snap and guide links already point at; the public materials that were on
`master` since 2020 stay on `archive-2020`. The program is GPL-3.0-or-later. The licensing module
(`src/modules/AccountAndLicense/`, `src/core/licensing/`, `src/services/licensing/`,
`src/ui/account_dialog.*`, `src/ui/plans_dialog.*`, `tst_daily_allowance`, `tst_legacy_account`)
is source-available under the Ktechpit Licensing Module License, with the same GPLv3 section 7
additional permission for official builds. `LICENSING.md`, `REUSE.toml` and SPDX headers record
it; the metainfo and the snap declare GPL-3.0-or-later (a `LicenseRef-proprietary` part made
Flathub show "Special License"); About carries the GPL's legal notice and a Licences link. The public README
and guide move to the root and `scripts/publish-public.sh` goes. The history is published whole;
the private repository stays as a frozen mirror. The snap workflow builds on `master`.

**Consequences.** Contributions to the GPL part are welcome; forks must leave the module out.
The public repository needs the `SNAPCRAFT_STORE_CREDENTIALS` secret and `PUBLISH_TO_STORE`
variable. The Flathub manifest still repackages the snap; building it from a tag is a follow-up.

## ADR-021: snap tags go straight to stable; edge builds name the commit (2026-11-03)

**Context.** Since 10.0 the snap workflow published every push to edge with the version of the
release it led to, and stable was promoted by hand in the store. The repository had no tags and
no GitHub releases. The owner asked for omnidict's flow (its ADR-021).

**Decision.** `.github/workflows/snap.yml`: a push to `master` publishes to edge with the version
`X.Y.Z+git<hash>` (set in `snap/snapcraft.yaml` when the build is not exactly the version's
tag); a `vX.Y.Z` tag publishes `X.Y.Z` straight to stable, after checking the tag against
`project(VERSION)`. A newer push to a branch cancels its older build, never a tag's.
`.github/workflows/release.yml` turns the same tag into a GitHub release whose notes are the
dated `CHANGELOG.md` section, and fails when the section is undated or the version differs.
Publishing stays gated by `PUBLISH_TO_STORE` and `SNAPCRAFT_STORE_CREDENTIALS`.

**Consequences.** Tagging is the decision to ship to every snap user; edge is where a build is
checked first. Candidate and beta stay unused. Flathub still follows the published snap by hand.

## ADR-022: two-finger swipe for back and forward, from the scroll the page leaves unused (2026-11-04)

**Context.** Public issue #18 asked for the browser's two-finger swipe back. A touchpad swipe
is horizontal scrolling, so it must not steal sideways scrolling from rows that use it, and TV
mode has no browser history (its Back leaves the screen, and stops a playing video).

**Decision.** `WebView` watches the touchpad gesture on its way into the page (its begin,
update and end phases) and counts only the sideways scroll the page hands back unused: Qt
WebEngine returns unconsumed wheel events to the view, without their phase. Chromium latches a
gesture to the scroller it started on and reports nothing once that scroller reaches its end,
so a row scrolled to its end never turns into Back mid-gesture, while a swipe that starts
where nothing can scroll sideways navigates. `core::SwipeTracker` holds the arithmetic (direction as
in Chrome: scrolling past the left edge is Back, whatever natural scrolling does to the
fingers; a mostly-sideways test, a threshold of a quarter of the width up
to 220 px) and decides 120 ms after the fingers lift, when the page's last answers are in.
`web::SwipeIndicator` is the arrow at the edge, as in Chrome: a bubble with a ring that grows with
the swipe, red once letting go would navigate, held red and faded when it fires. Setting `app/swipeNavigation`, default on; off in
TV mode, the mini player and full-screen video. Mouse wheels (no phase) never count.

**Consequences.** It needs scroll phases, which Wayland sessions report (KDE, GNOME); on X11 the
setting does nothing. Verified with synthetic gestures (`RED_DEBUG_SWIPE`) and an injected
sideways scroller, then on the owner's touchpad (2026-11-04), where the first version (Back =
fingers to the right, from `inverted()`) felt the wrong way round next to Chrome.

## ADR-023: Red's red for controls and selection, a softer focus border (2026-11-04)

**Context.** ADR-017 gave check boxes, radios, selection and the download dialog's picked card
YouTube's blue. The owner: red is Red's colour and must be the one used; the near-white border
of a focused field was too bright; the current Settings page's icon had a blue tint (Qt tints a
selected icon with the palette's highlight when the icon has no selected image).

**Decision.** `control` becomes `#FF0033` with white on it in both schemes: ticked check boxes,
radios, list check marks, text and list selection, the picked kind card, and the palette's
Highlight. Links stay YouTube blue (`link`), as on YouTube and the rest of the desktop.
`focusBorder` (`#8A8A8A` dark, `#909090` light) outlines a focused field instead of the text
colour, with `hoverBorder` (`#5A5A5A` / `#C4C4C4`) a step below. `icons::themed` always adds a
selected image (its normal colour unless told otherwise), so no icon is tinted by surprise; the
Settings pane's current page shows its icon in a light red (`#FF6B81` dark, `#FF0033` light).

**Consequences.** Supersedes ADR-017's blue `control`; red is no longer only for small marks.

## ADR-024: an AppImage on every release tag, built as whatsie's (2026-11-04)

**Context.** The owner asked for AppImage release builds, as whatsie publishes them, for
distributions without snap or Flatpak.

**Decision.** `.github/workflows/appimage.yml` runs on the same `vX.Y.Z` tag as `release.yml`
and `snap.yml`: on ubuntu-22.04 (widest glibc reach) it builds against the official Qt 6.11
binaries (`jurplel/install-qt-action`), bundles them with `linuxdeploy` and its Qt plugin, adds
the NSS modules Qt WebEngine dlopen()s, Deno 2.7.7 pinned by checksum next to the binary (the
download engine searches the application directory), and an AppRun hook that sets
`QTWEBENGINE_DISABLE_SANDBOX=1` (a read-only image cannot ship a setuid sandbox helper). It
checks the tag against `project(VERSION)` and then waits for the release `release.yml` creates
and uploads `Red-X.Y.Z-x86_64.AppImage` to it, so a refused tag gets no AppImage either.
The Wayland platform plugin is deployed on request, and its graphics integration
(wayland-egl), shell and decoration plugins are copied in by hand: without them the web view
cannot get an OpenGL context on Wayland and stays blank. `workflow_dispatch` with a `tag` rebuilds one for an existing release. Nothing is stored as a
workflow artifact. x86_64 only; no `.deb`.

**Consequences.** ffmpeg comes from the distribution, as everywhere (never downloaded). The
official Qt binaries lack the proprietary H.264/AAC codecs; YouTube falls back to VP9/AV1 and
Opus, so playback is unaffected, but this is the one build where that holds. The AppImage does
not update itself: users fetch the next release.

## ADR-025: "More apps from Ktechpit" from a shared module and a site endpoint (2026-11-04)

**Context.** The owner wants every Ktechpit app to show users the other apps, from one module
embedded in the apps' own widgets (About first), with the data served by the USSPublic site.

**Decision.** A separate repository, `ktechpit-apps` (MIT), is pulled into
`src/modules/KtechpitApps` with `git subtree` and builds the static `ktechpit_apps` library.
`AppCatalog` starts from a bundled copy of the catalog, then a cached download of
`https://ktechpit.com/USS/public/api/apps.php` (USSPublic `api/apps.php`, from
`includes/products.php` plus tags in `includes/app_tags.php`), refreshed 20 s after start at
most once every 7 days. It ranks apps for the host by shared tags, then trending rank, then site
order, leaving out the host, other platforms and apps installed here as a snap or Flatpak.
"Get" opens `snap://` or `appstream://` for the way the host was installed, else the product page
with `?ref=red-<place>`. Red shows two cards side by side in About under the link row, and its
*More apps* button opens the full list. `Settings::showOtherApps` (on by default; *Settings →
General*) turns every card off; with it off, *More apps* opens the products page as before.
The guide's Privacy section names the request.

**Consequences.** Module updates come with `git subtree pull --prefix src/modules/KtechpitApps
<repo> main --squash`. What Red suggests is decided on the site (tags, `promote`), not in Red.

