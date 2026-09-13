# Lessons — Red v9, TV mode, whatsie

The whatsie lessons (its `DOCS/LESSONS.md`, A1–A18, B1–B15, C, D, E) apply verbatim to a
Qt WebEngine wrapper and are not repeated. Below: what Red v9 and the TV-mode research add.

## A. Red v9 (2022–2023), from `reference/analysis-red-v9.md`

| # | Mistake | Consequence | Rule |
|---|---|---|---|
| R1 | Interceptor did substring scans over 630 rules, read `QSettings` twice and wrote a counter **per request**, appended a log line on the GUI thread | "the adblocker was laggy" | ADR-003: hash lookups on an immutable snapshot, atomic counter, nothing else per request |
| R2 | Blocked ~500 specific `googlevideo.com` edge hosts observed serving ads | region-specific playback breakage | Never block `googlevideo.com`; strip ads from responses instead |
| R3 | Metadata for every download came from the author's PHP endpoint | single point of failure, dead today | `-J` from yt-dlp; no Red servers |
| R4 | Parsed `-F` human table and progress text with regexes | broke on every yt-dlp release | ADR-005: JSON in, JSON out |
| R5 | Ran `python3 core`; never checked Python/ffmpeg/mpv/socat | downloads broken in the snap | ADR-004: standalone binary + qjs + ffmpeg, provisioned and verified |
| R6 | Version compare by (year > OR month > OR day >) | update prompts on the wrong days | compare `QDate`s |
| R7 | Pause = kill process; "finished" = `exitCode == NormalExit` (type confusion) | corrupted state | explicit `DownloadState`, `--continue` on resume, exit code + stderr tail |
| R8 | No cookies for yt-dlp | age-restricted/member content failed; today: bot check | ADR-006 |
| R9 | `--disable-web-security --no-sandbox` in release | security | sandbox on; no web-security flag |
| R10 | Firefox 72 UA on a Chromium engine | YouTube served a degraded UI | ADR-008 |
| R11 | Licensing, RateApp, trial files in `~/Downloads/.red.id`, password in base64 | trust | none of it |
| R12 | Theme by rewriting the `PREF` cookie and reloading | flicker, reload per toggle | drive `html[dark]` + `prefers-color-scheme` live |
| R13 | Only `watch?v=` recognised | youtu.be, shorts, playlists, channels not downloadable | `core::youtube_url` classifies every YouTube URL shape |

## B. TV mode (Leanback) research

| # | Observation | Rule for the Qt port |
|---|---|---|
| V1 | The whole TV-mode gate is the UA: `Mozilla/5.0 (PS4; Leanback Shell) Cobalt/…` — and it must be on **every** `www.youtube.com` request, split navigator vs wire | interceptor sets the header; profile UA for `navigator` |
| V2 | An Electron wrapper can rewrite CSP response headers (trusted types, style nonces, connect-src) — impossible in Qt WebEngine | build DOM without `innerHTML`; copy the page nonce onto injected `<style>`; do HTTP from C++ (ADR-007) |
| V3 | Early XHRs are held until hooks are installed | our hooks run at `DocumentCreation` before any page script, no holding needed |
| V4 | Leanback dispatches on `keyCode`, not `key` | gamepad/synthetic events set `keyCode` |
| V5 | `MutationObserver` on `<head>` re-appends styles so they win the cascade | same, cheap |
| V6 | `hide-shorts` matched the English string "Shorts" | match `iconType`/renderer shape, not text |
| V7 | A common gamepad map leaves the right stick as dead code (bug) | tests for the map |
| V8 | `visibilitychange` suppressed so YouTube never pauses on minimize | keep, as a setting default on |
