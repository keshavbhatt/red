# ROADMAP

| Milestone | Scope | Exit criteria |
|---|---|---|
| M0 Skeleton | CMake layers, settings, logging, single instance, CLI, theme service, brand style sheet, main window with rail + web view loading YouTube, tests green | `scripts/dev-build.sh --tests` passes; window shows YouTube |
| M1 Desktop mode | Navigation policy, pop-ups (sign-in), error page, fullscreen + hint, zoom, theme control on the page, autoplay, codec preference, tray, MPRIS, screen inhibit, mini player | Sign-in works; media keys work; mini player works |
| M2 Blocking | BlockList + interceptor, adblock.js/css, SponsorBlock client + script, hide shorts, blocked counter | A known ad-heavy video plays without ads; SponsorBlock skips with toast |
| M3 Downloads | EngineManager (yt-dlp/qjs/ffmpeg), MediaProbe, options → argv, DownloadJob/Queue with progress JSON, cookies export, DownloadDialog, DownloadsPanel, notifications | Video, audio-only, playlist download end-to-end from a fresh profile |
| M4 TV mode | Mode switch, UA split, tv scripts (h5vcc, identification, fixes, input, gamepad), 16:9 window, hint | Leanback loads, navigates with keyboard, plays, downloads current video |
| M5 Settings & polish | Settings dialog (7 pages), About, Shortcuts, diagnostics, first-run engine sheet, icons | Screenshots reviewed; no generic-Fusion look anywhere |
| M6 Packaging (last) | desktop/metainfo/icons, snap (kde-neon-6), CI | Snap installs and runs |
