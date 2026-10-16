# DESIGN — Red's UI/UX

Designed from scratch on 2026-09-06. Nothing here is inherited from Red v9's layout.
Principle: **the page is the product, the chrome is a frame**. Native UI never competes with
YouTube's own header/guide; it sits in a distinct brand surface and stays out of the way.

## 1. Brand

* Name: **Red**. Tagline: *YouTube, in a real desktop app.*
* Mark: a rounded-square tile in brand red with a white play glyph whose right edge
  carries a downward "tray" notch — play + download in one shape (`src/resources/icons/red.svg`).
  Symbolic variant: white glyph with a thin dark halo (tray/panel safe).
* Colour tokens (sampled from YouTube's 2026 design; the accent is YouTube red):

| Token | Dark | Light | Used for |
|---|---|---|---|
| `accent` | `#FF0000` | `#FF0000` | primary buttons, active rail item, progress |
| `accentHover` | `#FF3D3D` | `#E60000` | hover on primary |
| `accentText` | `#FFFFFF` | `#FFFFFF` | text on accent |
| `bg` | `#0F0F0F` | `#FFFFFF` | window, page background |
| `rail` | `#0F0F0F` | `#FFFFFF` | side rail (same as bg, 1px border) |
| `panel` | `#1F1F1F` | `#F9F9F9` | dialogs, downloads panel cards |
| `elevated` | `#282828` | `#FFFFFF` | menus, tooltips, popovers |
| `hover` | `#3F3F3F` | `#E5E5E5` | hover on neutral controls |
| `input` | `#121212` | `#FFFFFF` | text fields |
| `border` | `#303030` | `#E0E0E0` | hairlines |
| `text` | `#F1F1F1` | `#0F0F0F` | primary text |
| `muted` | `#AAAAAA` | `#606060` | secondary text |
| `link` | `#3EA6FF` | `#065FD4` | links (YouTube blue) |
| `success` | `#2BA640` | `#1E8E3E` | finished downloads |
| `warning` | `#F9AB00` | `#E37400` | paused / needs attention |
| `danger` | `#FF4E45` | `#D93025` | failed |

* Type: system UI font, Roboto preferred when installed (YouTube's face). Sizes 13/14 body,
  16 titles, 20 dialog headers. Weight 500 for titles, 400 body.
* Shape: 8 px radius on controls, 12 px on cards/dialogs, pill (18 px) on chips and primary
  buttons — YouTube's current rounded language.
* Motion: none that blocks; hover states only. Panel slide uses a 160 ms ease-out.

## 2. Main window (desktop mode)

```
┌──┬────────────────────────────────────────────────────┬─────────────┐
│  │                                                    │             │
│ R│                                                    │  Downloads  │
│ A│              YouTube (QWebEngineView)              │   panel     │
│ I│                                                    │ (slide-in,  │
│ L│                                                    │  380 px)    │
│  │                                                    │             │
└──┴────────────────────────────────────────────────────┴─────────────┘
```

**Left rail** (brand surface, 1 px border on the page side): 52 px icons only, or 200 px with
a label beside each icon (`window/railLabels`, default on; *Hide labels* / *Show labels* toggle
at the bottom, ADR-015). The icon keeps its x position in both widths; a disabled button can
carry a muted second line (mini player: "Play a video first"). The content scrolls (wheel) when
the window is shorter than the buttons. Top to bottom:

| Item | Icon | Action / shortcut |
|---|---|---|
| Red mark | logo | click → Home (`https://www.youtube.com/`) · Ctrl+H |
| **Mode switch** | YouTube / music / tv in one outlined group, active glyph in `accent` | Ctrl+1 / Ctrl+2 / Ctrl+3 select; Ctrl+M and Ctrl+T toggle Music and TV (ADR-014) |
| Back / Forward | chevrons | Alt+←/→ (disabled when unavailable) |
| Reload | ↻ | F5 (becomes ✕ "stop" while loading) |
| — | | |
| **Download** | ⤓ in a red pill when the page is a video/playlist/channel | Ctrl+D · opens the Download dialog for the current page |
| Downloads | list icon with a badge (active count) | Ctrl+J · toggles the panel |
| Mini player | picture icon | Ctrl+Shift+M |
| — spacer — | | |
| Settings | gear | Ctrl+, |
| Menu | ⋮ | F10 (also with the rail hidden) · About, Shortcuts, Open log folder, Quit |
| Labels toggle | sidebar glyph | *Hide labels* / *Show labels* |

The rail hides in fullscreen, in TV mode and in mini-player mode. Setting "Show side rail"
keeps everything reachable via shortcuts and the tray.

**Downloads panel** (right, 380 px, `panel` surface, border-left). Docked beside the page from
900 px of window width; narrower, it floats over the page's right side (Esc, Ctrl+J or ✕ close
it) and is not reopened at start. Where it cannot show (full screen, mini player, TV, or closed in
a narrow window) a queued download raises a hint: "Download added: <title>" or "Already in your
downloads: <title>".
* Header: "Downloads" · engine chip (● yt-dlp 2026.08.19 / "Setting up…" / "Update available")
  · `+ New` (paste-URL dialog) · ⋮ (Pause all, Resume all, Clear finished, Open folder, Engine…).
* Cards (88 px): 96×54 thumbnail with a state-coloured 3 px left bar, title (2 lines),
  channel · quality/format line, progress bar (accent) with "12.4 MB of 118 MB · 3.2 MB/s · 00:32"
  or "Merging…" / "Finished · 118 MB" / "Failed · reason". Hover actions: pause/resume, cancel,
  open, show in folder, retry, remove. Double-click opens the file.
* Footer: "Save to  ~/Downloads/Red  [Change]".
* Empty state: illustration glyph + "Downloads you start will show up here" + "Paste a link".

## 2b. Naming the internals

Users never meet tool names. yt-dlp and its script runtime are "the download engine" (shown
with a version); ffmpeg is "the media converter". The real names appear only where the user
has to act on them: the install hint for a missing converter ("install the ffmpeg package:
sudo pacman -S ffmpeg"), the advanced "use an engine already installed on this system
(yt-dlp)" option, and Diagnostics.

## 2c. Message sheets

Every question or notice Red asks is a `ui::MessageSheet` (never QMessageBox / QInputDialog /
the engine's stock JavaScript boxes): 44 px accent badge (amber for warnings), title, body,
optional muted note and text field, pill buttons with one accent primary. Used for the
permission prompt, "Download from link", clear session, reset settings, the GPU notice, the
crash notice, and the page's alert / confirm / prompt (`WebPage::setScriptDialogs`); Chromium's
"leave this page?" guard becomes "Leave this page?" with Stay / Leave.

## 3. Download dialog ("Save video")

Modal, 640 px wide, `panel` surface, 12 px radius.

1. Hero row: 160×90 thumbnail · title (2 lines, 16/500) · channel · duration · views/date.
   While probing: a shimmer placeholder + "Reading video info…"; errors inline with Retry.
   A watch link that carries `list=` is read as both scopes at once (video full, playlist
   flat) and a card under the hero offers "Just this video" / "Whole playlist (N videos)" with
   the playlist title; switching is instant from the cached reads, and a playlist that cannot be
   read leaves the option disabled with the reason and Retry. Mix ids (`RD…`) are not playlists.
2. Kind chooser (`ui::KindCard` ×3, icon + title + hint, accent border on the pick):
   **Video** "Picture and sound" · **Audio only** "Just the sound, for music" · **Advanced**
   "Pick the exact streams". Opens on the kind of the last download (`downloads/lastKind`);
   the page context menu also offers "Download audio only…" which opens straight on Audio.
   The Download button reads "Download video" / "Download audio" / "Download".
   * Video: Quality (Best available / 2160p / 1440p / 1080p / 720p / 480p / 360p, greyed when
     the video has no such stream) · Container (MP4 / MKV / WebM) · Subtitles (None / language
     list incl. auto-generated, "Embed" toggle) · toggles: Embed thumbnail, Embed metadata &
     chapters, Remove sponsor segments.
   * Audio: Format (Best / MP3 / M4A / Opus / FLAC / WAV) · Quality (Best / 192 / 128 kbps)
     · Embed thumbnail (cover art) · Embed metadata.
   * Advanced: table of all formats (id, type, resolution/bitrate, fps, codec, HDR, size) with
     one video and one audio selectable; or a single progressive/audio row.
3. Playlist/channel: an entry list with checkboxes (thumbnail, title, duration), "Select all",
   range "from … to …", live count "24 of 120 selected". Options apply to all entries.
4. Footer: Save to folder (path + Change) · filename preview ("Title [id].mp4") ·
   `Cancel` · **`Download`** (accent pill).

## 4. Settings

Dialog 860×600, left navigation list (icon + label) on `bg`, content on `panel` in a scroll
area with grouped cards (title 14/500, rows label-left control-right, 12 px gaps):

* **General**: Open Red in (YouTube / YouTube Music / TV mode / Last used), YouTube start page (Home / Subscriptions / Last visited), When closing (Quit / Keep in
  tray), Show side rail, Notifications on download finish.
* **Appearance** — Theme (System/Light/Dark), Page zoom, Interface scale (restart).
* **Playback** — Autoplay without click, Preferred codec, Hardware video decoding (restart),
  Keep screen awake while playing, Mini player size.
* **Blocking** — Block ads, Block trackers, Hide Shorts, blocked-request counter;
  SponsorBlock on/off + category grid (Skip / Show / Off per category), Show segment markers.
* **Downloads** — Folder, Filename pattern, Default quality, Default container, Default audio
  format, Concurrent downloads, Speed limit, Use my YouTube sign-in, Embed thumbnail, Embed
  metadata & chapters, Remove sponsor segments; **Engine card**: yt-dlp version · JS runtime ·
  ffmpeg, "Check for updates", "Update now", "Auto-update daily", "Use system yt-dlp" (path).
* **TV mode**: Fullscreen in TV mode, Gamepad support, Hide Shorts in TV,
  Unlock 4K, Low-memory mode; a shortcuts card.
* **Advanced** — Hardware acceleration, User agent override, Clear cache, Sign out & clear
  session, Open log folder, Copy diagnostics, Reset settings.

## 5. TV mode

The window drops the rail and status hints, keeps 16:9 content (letterboxed on other
ratios with `bg`), enters fullscreen (setting), and the page becomes Leanback. A 3-second
hint window "TV mode · Esc back · Ctrl+T: back to YouTube · Ctrl+D download" appears on entry (the
same mechanism as the fullscreen hint). Ctrl+T returns to desktop mode and restores geometry.
A *Back to YouTube* pill (top left, `ui::TvExitButton`) shows on entry and on every mouse
move, fading after 3 s; it is the visible way out while the rail is hidden.

## 6. Mini player

Ctrl+Shift+M shrinks the window to fit what plays (setting: small/medium/large): 480×270 for a
video, 270×480 for a Short (9:16, re-fitted when Shorts and videos swap), 360×640 for YouTube
Music, whose own responsive player page fills it (`core::mini_player_policy`). Always-on-top,
rail hidden, page in `red-mini` state (masthead/guide/comments hidden, player fills the
viewport, controls on hover). Ctrl+Shift+M, Esc, the tray, the window's close button or the
round restore button in the top right corner (`ui::OverlayButton`, shown on entry and on every
mouse move, hidden after 3 s) restore the previous geometry. Closing the mini player never quits;
Quit (menu, tray, Ctrl+Q) still does.

## 7. Empty / error / first-run states

* First launch: no wizard. The engine is set up lazily on the first download with a
  progress sheet ("Setting up the download engine — yt-dlp · JavaScript runtime · ffmpeg").
* Load failure: branded error page (bg, mark, title, reason, Try again).
* GPU fallback notice: one-time non-modal banner in the downloads panel header area.
