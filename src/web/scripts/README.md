# Injected scripts

JavaScript injected into YouTube (desktop), YouTube Leanback (TV mode) and YouTube Music (Music mode). Rules:

* One file per concern, registered in `../CMakeLists.txt` via `qt_add_resources`, installed by
  `web::ScriptBundle` on the **profile-level** script collection at `DocumentCreation`.
* `bootstrap.js` runs first with the config object and `qwebchannel.js` prepended by C++; it
  creates `window.__red` (`config`, `report()`, `log()`, `bridge`, `onBridge()`, `onConfig()`).
* `hooks.js` runs second and offers `__red.hooks.onJson / onRequest / onInitial / prune`.
* Every other file starts with a header block (name / purpose / depends / verified / on-fail),
  is wrapped in an IIFE with `try/catch`, and reports failures through
  `window.__red.report(name, error)` → `web::Bridge::scriptFailed` → `red.web.js` log.
* Only the *stable tier* is allowed (CODING_STANDARDS §7): standard APIs, our own CSS,
  InnerTube field names, YouTube's public player API, semantic element tags.

| File | Mode | Feature |
|---|---|---|
| `bootstrap.js` | both | infrastructure |
| `hooks.js` | both | JSON / fetch / XHR / initial-data modifier registry |
| `adblock.js` | both | B2, B3 |
| `sponsorblock.js` | both | B6, T10 |
| `media-session.js` | both | D7, D8, tray controls |
| `hide-shorts.js` | both | D9, T11 |
| `codec-preference.js` | both | D4 |
| `background-play.js` | both | D7 (keep playing when hidden) |
| `theme-control.js` | desktop | D1 |
| `mini-player.js` | desktop | D6 |
| `tv/h5vcc.js` | tv | T6 |
| `tv/identification.js` | tv | T7 |
| `tv/leanback-fixes.js` | tv | T8, T15 |
| `tv/input.js` | tv | T4, T14 |
| `tv/gamepad.js` | tv | T5 |
| `music/download-menu.js` | music | M9 |

Bridge API (`web::Bridge`): JS→C++ `scriptFailed`, `log`, `mediaState`, `requestSponsorSegments`,
`sponsorSkipped`, `tvSignal`, `retry`, `downloadRequested`; C++→JS `sponsorSegments`,
`mediaCommand`, `configChanged`, `miniPlayerChanged`.

Diagnostics: `RED_DISABLE_WEB_SCRIPTS=all` (or a comma list of names) drops scripts.
