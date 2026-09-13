# Red (p-red) — Feature and Technical Inventory

Source: `/home/commander/DCode/p-red` (git history: 10 commits, 2022-03-06 to 2023-09-06; last tag "version 9.0"). About 6,000 lines of C++/UI. Everything below is from reading the full source.

---

## 1. Build system, Qt version, modules, file list

**Build:** qmake, `src/red.pro`. `QT += core gui widgets webengine webenginewidgets webchannel xml`, `CONFIG += C++11`, `LIBS += -lX11` (for CapsLock detection in the lock screen). `VERSION = 9.0` exported as `VERSIONSTR` define. `include(RateApp/RateApp.pri)`. Release builds define `QT_NO_DEBUG_OUTPUT`. Target installs to `/usr/bin/red`. Qt 5.15.2 (from beineri PPA in the snap). `webchannel` is linked and `js/qtwebchannel.js` is bundled, but **QWebChannel is never actually used** (only included).

**Source files (`src/`):**

| File | Purpose |
|---|---|
| `main.cpp` | Sets `QTWEBENGINE_CHROMIUM_FLAGS` (`--no-sandbox --disable-logging --disable-web-security` in release; `--remote-debugging-port=9422` in debug), HiDPI, app name `Red`, org `org.keshavnrj.ubuntu`, RunGuard single-instance, global WebEngine settings (plugins, DNS prefetch, fullscreen, clipboard), `ForcePersistentCookies`. |
| `mainwindow.cpp/.h/.ui` | Main window: QSplitter with web view (left, stretch 4) + download/side panel (right, stretch 2); all QActions/shortcuts; settings dialog logic (`settings.ui`); theme cookie injection; history capture; fullscreen handling; download request flow; engine wiring. |
| `webengineview.cpp/.h` | `WebEngineView` (QWebEngineView subclass): custom profile, user agent, ad-block interceptor, injected CSS/JS, context-menu rewrite, error page, theater mode script, render-crash dialog. Also defines `RequestInterceptor` (the adblocker). |
| `requestinterceptor.h` | Misnamed: actually `WebEnginePage` subclass; `createWindow()` opens popups (Google sign-in) in a new dialog `QWebEngineView` sharing the same profile. |
| `engine.cpp/.h` | "Engine" = yt-dlp binary management: presence check, download, version check/update prompt, cache clear. |
| `videooption.cpp/.h/.ui` | "Video Option" dialog: runs `yt-dlp -F`, parses formats into radio buttons, Watch (mpv) / Download / Listen / Download audio. |
| `download_widget.cpp/.h/.ui` | `Widget` = side panel: toolbar (back/home/forward/reload/red download/account/lock/settings/detach/menu), Downloads tab (list of yt-dlp QProcesses w/ progress), "Recently 30 Visited" history tab; flat-file persistence. |
| `downloaditem.ui`, `history.ui` | Item widgets for the two list views. |
| `manifest_resolver.h` | Header-only DASH manifest parser (finds `audio/mp4` BaseURL). **Unused** in current code. |
| `request.cpp/.h` | Simple QNetworkAccessManager GET wrapper with disk cache and Chrome UA; used to call the ktechpit metadata endpoint. |
| `account.cpp/.h/.ui` | Licensing: random account id, `.red.id` file, 10-day evaluation, remote purchase check. |
| `restorepurchase.cpp/.h/.ui` | Stub dialog "Restore Old Ktube licence" (email + transaction id; restore button does nothing). |
| `lock.cpp/.h/.ui` | App lock screen (password stored base64 under key `asdfg`), X11 CapsLock indicator. |
| `blocked.cpp/.h/.ui` | "Blocked requests" log window with counter; auto-clears log every 2 min. |
| `urlform.cpp/.h/.ui` | Small URL dialog with 3 modes: OpenUrl, DownloadCurrent, NewDownload. |
| `slider.cpp/.h/.ui`, `slides/*.html` | Intro/help slideshow (7 rich-text slides, 10 s auto-advance). |
| `about.cpp/.h/.ui` | About dialog with debug info, donate/rate/more-apps links. |
| `RateApp/` | "Rate this app" nag (after 5 launches or 4 days, 30 s delay; opens `snap://red`). |
| `rungaurd.cpp/.h` | QSharedMemory + QSystemSemaphore single-instance guard. |
| `utils.cpp/.h` | Cache size/dir delete, random id, seconds-to-days, html-to-plaintext, debug info. |
| `remotepixmaplabel2.cpp/.h` | QLabel that loads a remote thumbnail with fallback + retry button. |
| `elidedlabel.*`, `controlbutton.*`, `waitingspinnerwidget.*` | UI helpers (eliding label, hover-cursor button, spinner). |
| `dark.qss`, `light.qss`, `breeze.qrc`, `dark/`, `light/` | Breeze-style Qt stylesheets (only `dark.qss` is applied; Fusion style + hardcoded dark palette). |
| `js/skip.js`, `js/ads.light`, `js/ads.txt`, `js/qtwebchannel.js` | Injected ad-skipper; block lists (light = 629 lines used; full = 12,297 lines unused). |
| `css/scroll.css`, `css/core.css` | Injected page CSS (only `scroll.css` used). |
| `red.desktop`, `red.ico`, `icons/` | Desktop entry and icon set. |

---

## 2. Complete user-facing feature inventory

**Window/layout:** single QMainWindow; horizontal splitter = YouTube web view + right side panel (download manager/history). Side panel is detachable into a floating `Qt::Tool` window ("Detach"/"Restore" button, `midi`). Home URL: `https://www.youtube.com/?app=desktop`. First run maximizes. Geometry saved (`geometry`). Window title = `Red v9.0 | <page title>`.

**Main menu (popup from panel "Menu" button) and shortcuts:**
- Minimize `Ctrl+M`; Toggle Fullscreen `F11`; Lock `Ctrl+L`
- Download loaded URL `Ctrl+D`; New Download from URL `Ctrl+N`
- Open Url `Ctrl+O`; Back `Ctrl+Left`; Forward `Ctrl+Right`; Home `Ctrl+H`; Reload `F5`
- Account; Settings; Help (intro slides); About; Quit `Ctrl+Q`
- `Esc` (global event filter): stops page load and cancels pending metadata request.

**Side panel toolbar:** reload (replaced by red spinner while loading), back, home, forward, red "download" button (turns red `#cb0000` when hovering a `watch?v=` link), account (hidden), lock (hidden), settings, detach, menu.

**Downloads tab:** list of download items (thumbnail `https://i.ytimg.com/vi/<id>/mqdefault.jpg`, title, status, size, speed, ETA, progress %, colour bar: red error / green running / orange paused / blue finished), buttons Pause/Start/Remove (selected) and Pause All/Start All/Remove All, stats line "running | paused | stopped | finished", video and audio download location fields with "Change download location" pickers, "Download already added to the list!" fade-in notice. Double-click an item plays the file with mpv (`--force-window --geometry=800x600+0+0 --no-ytdl --volume=50`) or shows "unable to locate file / Browse". All downloads paused on app quit (`aboutToQuit -> on_pauseAll_clicked`).

**History tab ("Recently 30 Visited"):** thumbnails + title captured from `loadFinished`/`titleChanged` when URL contains `watch?v=`; double-click loads; Clear History; thumbnail width setting.

**Settings dialog (`settings.ui`), keys in `QSettings("org.keshavnrj.ubuntu","Red")`:**
- Download Engine: Status label (Present/Absent/Downloading...), "Download/Update" button, "Clear" engine cache ("if you get 403 or 421").
- `lockscreen` (bool, "Enable App Lock on Start"), `asdfg` (base64 password), shows current password in clear text.
- `restore_last_page` (bool, default true) with `lastVisitedUrl`.
- History: Clear, `historyThumbnailWidth` (int, default 120; "Default size" button).
- Ad Blocker: `adblocker` (bool, default true), `eventlogger` (bool, default true, "Disable Youtube Event logger (Youtube Tracker)"), "See blocked requests" button -> Blocked window (counter `adblocker_blocker_count`).
- YouTube: `dark`/`light` radio (bool pair, default dark), `cinema` (bool, default true, "Force Youtube Player Cinema mode"), `autoplay` (bool, default true, labelled "Disable Auto playback"; maps to `PlaybackRequiresUserGesture`).
- Cache & Cookies: shows sizes of profile `cachePath()` and `persistentStoragePath()`, Clear buttons (recursive dir delete).
- Other keys: `firstrun`, `shownIntro`, `setting_path`, `App`, `download_path`, `download_path_audio`, `preferedQuality` (0 high/1 medium/2 low), `accountId`, `Red` (base64 "activated"/junk), `Red_emit` (base64 evaluation start epoch), RateApp keys `app_launched_count`, `app_install_time`, `rated_already`.

**Themes:** Qt side is always dark (`dark.qss` + Fusion palette). YouTube dark/light is done purely by setting the `PREF` cookie via JS: dark = `document.cookie='PREF=f6=400&f5=30000;path=/;domain=.youtube.com'`, light = `PREF=f6=80000&f5=30000`, then reload. Also injected on first run at 70% load progress.

**App lock:** password >4 chars, set/confirm screen, login screen overlaying the main window; CapsLock indicator via XkbGetIndicatorState; lock on start if `lockscreen`.

**Licensing (Evaluation/Pro):** 10-day trial (`ev_time = 864000` s). After trial, download is blocked with "Evaluation period ended" dialog; trial also limits to 2 concurrent downloads. Account id (20-char random) stored in settings and in `~/Downloads/.red.id` (with base64 trial-start timestamp appended) so it survives config wipe; also `.dbn` file in data dir. Purchase check: `GET http://www.ktechpit.com/USS/RED/checkout/chkout/check.php?username=<id>` -> response containing "Account is active" = Pro. Buy in-app loads `http://ktechpit.com/USS/RED/checkout/index.php?accountId=<id>` in the web view (a `resetBuyNow` signal fires when `checkout/index.php` loads); or external browser. Support email 98378sub@gmail.com.

**Not present:** no system tray, no notifications, no MPRIS/media keys, no picture-in-picture, no mini-player, no SponsorBlock, no tabs, no youtu.be handling, no cookie export to yt-dlp.

---

## 3. Download engine (exactly how it worked)

**Binary:** yt-dlp (previously youtube-dl; the old URLs are left in comments: `https://rg3.github.io/youtube-dl/update/LATEST_VERSION`, `https://yt-dl.org/downloads/latest/youtube-dl`, and a self-hosted `http://ktechpit.com/USS/engine/core.eco` / `core_version`).
- `ENGINE_DOWNLOAD_URL = https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp`
- `ENGINE_VERSION_URL = https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest` (reads JSON `tag_name`).
- Stored as `<QStandardPaths::DataLocation>/core` (i.e. `~/.local/share/org.keshavnrj.ubuntu/Red/core`, or under `$SNAP_USER_DATA` in snap). Version cached in `<DataLocation>/core_version`.
- Always executed as `python3 <DataLocation>/core <args>` (never chmod'd, never run directly). So Python 3 must exist on the host / in the snap.
- Download: QNetworkAccessManager GET with manual 3xx redirect following (the GitHub URL redirects). On success it writes the file, runs `--rm-cache-dir`, fetches version JSON, runs `core --version`.
- Update check on every start (1 s after launch): compares local `core_version` vs remote `tag_name` by parsing both as ISO dates (`QDate::fromString(..., Qt::ISODate)`) and flags update if `n_year > year || n_month > month || n_day > day` (buggy comparison: any later day-of-month triggers). If no local version file, assumes `2019.01.01`. Shows an Ok/Cancel/Quit message box; Ok opens Settings and clicks Download. If binary absent it auto-downloads silently.
- "Clear engine cache" = `python3 core --rm-cache-dir` (documented as fix for 403/429).

**Metadata step (before formats):** on "Download and Play Option" the app does NOT ask yt-dlp for metadata; it calls the author's server: `GET http://ktechpit.com/USS/Olivia/invidio_video.php?video_id=<id>` (shared with his Olivia music app), with Chrome 73 UA. Response is a `<==>`-separated string with 10 fields: `songId, title, album, artist, coverUrl, ytIds (may contain "<br>"), base64, dominantColor, artistId, albumId`. If fewer than 10 fields -> "No Valid Meta" error. This server dependency is a single point of failure.

**Format list:** `python3 core -F https://www.youtube.com/watch?v=<id>`; stdout is split at the word "resolution" (header row), then line by line:
- lines containing "audio only" -> code = first token, then parses ext/resolution/size by fragile string splitting (`split("|").at(1)` for size) -> radio button `"<res>(<size>)"` objectName `audio-<code>`.
- lines containing "video only" -> code, ext, resolution, fps, size -> radio `"<res>(<size> <fps>fps)"` objectName `video-<code>`; one per unique resolution.
Preferred quality radios (highest/medium/lowest, saved as `preferedQuality`) auto-select last/middle/first entries. TODO.md notes "[engine] output of formats showing properly" as a known issue, and commit "fix format parsing" — the parser broke on yt-dlp's table format (columns, `|` separators, "~" sizes). Progressive (combined) formats are ignored entirely.

**Watch (stream without downloading):** `python3 core -f <video>+<audio> --get-url <url>` -> two URLs -> `mpv --wid=<winId> --title=... --no-ytdl <videoUrl> --audio-file=<audioUrl> --input-ipc-server=<fifo> --volume=50` embedded in the dialog. Fullscreen state polled every 500 ms via `echo '{"command":["get_property","fullscreen"]}' | socat - <fifo>` (fifo under `<TempLocation>/Red_sockets/<epoch>.fifo`, files older than 10 h purged). Web view is muted while mpv plays. **Listen:** `-f bestaudio --extract-audio --get-url` -> mpv `--force-window`.

**Download command:**
- Video: `python3 core -f <videoCode>+<audioCode> -o "<downloadLocation>/<title>.%(ext)s" <url>`
- Audio: `python3 core -f bestaudio --add-metadata --extract-audio --audio-format <mp3|m4a|flac|aac|wav|opus|vorbis> --audio-quality 0 --prefer-ffmpeg -o "<audioLocation>/<title>.%(ext)s" <url>`
- Title sanitised only by removing `.`, `/`, `"`; suffixed " [Video]" / " [Audio]". Default locations `~/Downloads/Red` and `~/Downloads/Red/Music`. Merging relies on ffmpeg being on PATH (never bundled or checked). No `--newline`, no `--progress-template`, no rate limit, no playlist support, no cookies/`--cookies-from-browser`, no subtitles, no thumbnail embedding (commented out).
- Duplicate URL detection via `database/database.db` (`<uuid> >> <base64 url>` lines); per-item state files in `history/<uuid>` (args line `"<args> -o<-->dest<==>url"`, uuid, status, progress, size, title, percent, downloaded, colour stylesheet). On startup every saved item is recreated as a not-started QProcess with same args (resume = re-run yt-dlp, which resumes `.part` files).
- **Progress parsing** from stdout chunks containing `[download]` but not `Destination:`/`Merging formats into`/`Resuming download`/`[ffmpeg]`/`fragments`: regex `(\d+\.\d+%)` for percent, speed = text between " at " and " ETA", size = between "% of " and " at ". Downloaded bytes = percent × total.
- Pause = `QProcess::close()` (kills process); Start = `p->start()` again. Errors: if stderr/stdout contains "403", shows dialog suggesting engine cache clear. Finished is judged by `exitCode == QProcess::NormalExit` (i.e. exitCode == 0, a type confusion that happens to work).
- Session history (`session/session`, base64 lines `url<=>title<=>imgsrc`, capped to 30).

---

## 4. Adblocker

Implemented as a `QWebEngineUrlRequestInterceptor` (`RequestInterceptor` in `webengineview.h`) set on the custom profile, plus cosmetic CSS and a click-the-skip-button script.

- Rule source: `:/js/ads.light` (629 lines, bundled, never updated from the network). It is a plain substring list: e.g. `annotations_module`, `ads?`, `adunit`, `youtube.com/pagead`, `youtube.com/adpage`, `doubleclick.net/`, `googlesyndication.com`, `googleadservices.com`, `google-analytics`, `googletagmanager`, `2mdn.net`, `innovid.com`, `moatads`, `fwmrm.net`, `youtube.com/get_video_info`, `youtube.com/get_midroll_info`, `youtube.com/youtubei/v1/log_event`, `youtube.com/ptracking`, `youtube.com/api/stats/`, `ad_break?`, `/pagead/`, `ad_cpn`, `s.ytimg.com/`, `www.youtube-nocookie.com`, and ~500 hard-coded `rN---sn-xxxx.googlevideo.com` CDN edge hostnames (the author blocked specific googlevideo hosts observed serving ad media — inherently fragile and region-specific). Lines starting with `#` are NOT treated as comments (they're just non-matching strings). `ads.txt` (12,297 lines, mostly more googlevideo hosts) is bundled but unused.
- When `eventlogger` is on, `/log_event`, `/ptracking`, `/api/stats/`, `/stats` are appended.
- Whitelist: any URL containing `/accounts`, `accounts.youtube`, or `signin=` is never blocked (login fix).
- Match = `reqUrlStr.contains(rule, CaseInsensitive)` for each rule, linear scan; blocked via `info.block(true)` and `blocked(url)` signal -> Blocked log window (`adblocker_blocker_count` persisted, incremented for every block — a QSettings write per blocked request).
- **Why laggy:** on every request it (a) re-reads QSettings twice, (b) appends the whole 629-entry store to the working list then `removeDuplicates()` (O(n²)-ish on every request; the list also grows since `adsUrl.append(adsUrlStore)` runs every call before dedupe), (c) linear substring scan over ~630 rules, (d) emits a signal that triggers a QSettings write and QTextBrowser append on the UI thread. In Qt 5.13+ interceptors run on the IO thread, but the signal/log work lands on the GUI thread; the settings writes and unbounded log appends are the main lag sources. Also blocking `youtube.com/get_video_info` and `s.ytimg.com/` can break player features.

---

## 5. Injected JavaScript / CSS

All injected via `QWebEngineScript` at `Deferred`, `ApplicationWorld`, `runsOnSubFrames = true`:

1. **`skipper` (`js/skip.js`):** every 300 ms, `document.getElementsByClassName(...)` and `.click()` on `ytp-ad-skip-button-text`, `ytp-ad-overlay-close-button`, `ytp-ad-skip-button`.
2. **`scrollbars` (`css/scroll.css`, injected as `<style id="scrollbars">`):** red `#cb0000` webkit scrollbars on `#161616` track; hides ad/promo DOM: `.ytp-ad-image-overlay`, `.video-ads .ytp-ad-module`, `[class*=promoted]`, `[id*=promoted]`, `[id*=promo]`, `[id*=-ad-]`, `#player-ads`, `.ytp-ad-overlay-container`, `#masthead-ad`, `ytd-video-masthead-ad-v3-renderer`, `ytd-promoted-sparkles-text-search-renderer`, `ytd-promoted-video-renderer`, `ytd-compact-promoted-video-renderer`, `ytd-merch-shelf-renderer`, `ytd-mealbar-promo-renderer`, `.ytp-pause-overlay`, plus legacy ids (`#feed-pyv-container`, `#watch-branded-actions`, etc.). Hides links to `youtubekids.com`, `music.youtube.com`, `tv.youtube.com`. Disables text selection/drag on `body, a, img`, `overflow-x:hidden`.

Run via `runJavaScript` on each `loadFinished`:
3. **Theater mode** (if `cinema`): `window.addEventListener('yt-navigate-finish', ...)` -> if `document.querySelector('button.ytp-size-button')` exists and `document.getElementById('player-theater-container').firstChild === null`, click it; also sets cookie `wide=1;path=/;domain=.youtube.com` (or `wide=0` when disabled).
4. **Theme cookie** `PREF=f6=400&f5=30000` (dark) / `PREF=f6=80000&f5=30000` (light) in MainWorld.
5. **Error page:** on `loadFinished(false)`, `page()->toHtml()` is inspected for `<div class="error-code" jscontent="errorCode"` and replaced with a custom dark HTML page containing a `location.reload()` link.
6. `sendKeyEvent(keyCode)` dispatches a synthetic `KeyboardEvent('keydown')` (defined but unused).

No script talks to YouTube's InnerTube API; the only endpoints used from C++ are `i.ytimg.com/vi/<id>/mqdefault.jpg` thumbnails and the ktechpit PHP endpoints.

---

## 6. YouTube-specific hacks

- **User agent:** profile UA `Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:72.0) Gecko/20100101 Firefox/72.0` (Firefox spoof to allow Google login in an embedded Chromium and avoid the "this browser may not be secure" block). Metadata requests use a Chrome 73 UA.
- **Profile:** named persistent profile `QWebEngineProfile("Red")`, `ForcePersistentCookies`, LocalStorage, plugins, `LocalContentCanAccessRemoteUrls/FileUrls`, `XSSAuditingEnabled`, `FocusOnNavigationEnabled=false`, `ScrollAnimatorEnabled=false`, `PlaybackRequiresUserGesture` tied to `autoplay`. Chromium flags `--disable-web-security --no-sandbox` in release.
- **Login:** popups (`WebBrowserWindow`) from `createWindow` open in a separate 500x500 dialog `QWebEngineView` on the same profile (Google OAuth popups). Adblock whitelist for `accounts`/`signin=`. TODO notes 2FA not verified.
- **Consent:** nothing explicit (no CONSENT/SOCS cookie handling); only the `PREF` and `wide` cookies.
- **Fullscreen:** `fullScreenRequested` accepted; hides the side panel, hides scrollbars, sets window fullscreen, restores previous state on exit.
- **Context menu:** standard menu with SavePage/ViewSource/DownloadImage/CopyImage/OpenInNewWindow hidden; `OpenLinkInNewTab` is renamed "Download and Play Option" and only shown while hovering a `watch?v=` link (`linkHovered` stores `VIDEO_URL`). Profile `downloadRequested` (native downloads) is redirected to the same flow.
- **URL handling:** only `watch?v=` is recognised; video id = `split("watch?v=").last().split("&").first()`. No youtu.be, shorts, playlist, or external-link handling; any URL typed in "Open Url" is loaded in the same view. Non-YouTube navigation is not blocked.
- Native `ShowScrollBars` toggled off during load and on after 90%.
- Render-process crash -> "reload?" dialog.

---

## 7. Snap packaging

`snap/snapcraft.yaml`: name `red-app`, version `9.0`, `base: core20`, `confinement: strict`, `grade: stable`, `compression: lzo`, amd64 only, icon `snap/gui/icon.png`. Uses the author's `qt515-core20` content snap (`SNAP_DESKTOP_RUNTIME=$SNAP/qt515-core20`, launcher `bin/desktop-launch` from `github.com/keshavbhatt/qt515-core20`) and the beineri Qt 5.15.2 focal PPA at build time (`qt515base qt515webengine qt515xmlpatterns`). App command `bin/desktop-launch $SNAP/usr/bin/red`, env `IS_SNAP=1`, `QT_QPA_PLATFORMTHEME=gtk3`, `DISABLE_WAYLAND=1`. Plugs: `avahi-observe, browser-support, camera, desktop, desktop-legacy, gsettings, home, opengl, audio-playback, audio-record, removable-media, screen-inhibit-control, unity7, upower-observe, x11, mount-observe, network, wayland`; content plugs `gtk-3-themes`, `icon-themes`, `sound-themes`, `qt515-core20`. Desktop file `red.desktop` (`Name=Red`, `Exec=red`, `Icon=red` rewritten to `${SNAP}/meta/gui/icon.png`, `Categories=Utility;Qt;Player;`, `Keywords=Downloader;Youtube;Tv;You`). Note the snap does not stage python3, ffmpeg, mpv or socat — they are all assumed to come from the base/host, which is why downloads/watch were unreliable under strict confinement.

---

## 8. Other notable items

- **Single instance:** `RunGuard("org.keshavnrj.ubuntu.Red")` (SHA1-keyed QSharedMemory/QSystemSemaphore); second launch shows a critical box and exits (no URL forwarding to the running instance).
- **Network cache:** `QNetworkDiskCache` at CacheLocation shared for thumbnails.
- **RateApp nag** and **intro slideshow** (`shownIntro`) on first run; slides advertise: right-click title/thumbnail to download, Recent 30 history, video/audio options, navigation.
- **About:** author Keshav Bhatt, `http://ktechpit.com`, donate `https://paypal.me/keshavnrj/10`, source link `https://github.com/keshavbhatt/red` (marked closed-source: `isOpenSource=false`), rate link `snap://red-app`.
- **Known defects / limitations worth not repeating:** password stored base64 and shown in plain text; trial state in `~/Downloads/.red.id`; format parser depends on yt-dlp's human table; date-based version comparison is wrong; server-side metadata dependency (`invidio_video.php`) for every download; "Finished" detection via exit code; pausing kills the process; per-request QSettings writes in the interceptor; hard-coded googlevideo host blocking; `--disable-web-security`; no cookies passed to yt-dlp so age-restricted/member content fails; no notifications/tray/MPRIS/PiP.
