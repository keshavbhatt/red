# Red — project instructions

Red is a proprietary YouTube desktop client (Qt 6.11, C++20, CMake). Read `DOCS/` for the
feature contract (FEATURES), design (DESIGN), decisions (DECISIONS, ADRs) and progress log.
The repository is private; `DOCS/` is committed.

## Standing rules

- **Never mention accounts or licensing in user-facing release text.** That means no accounts,
  licences, Pro plans, evaluation/trial, checkout, activation or the licence server in
  `CHANGELOG.md`, the metainfo `<releases>` notes, the What's new dialog, README/store copy, or
  anywhere else a user reads about a release. Licensing work is described only in commit
  messages and `DOCS/`. A unit test (`tst_changelog`) enforces this for the changelog.
- No commit or PR attribution lines (no Co-Authored-By, no session links).
- User-facing text never names yt-dlp, Deno, QuickJS or ffmpeg: say "download engine" and
  "media converter". Real names only in install hints, the advanced system-engine option, and
  Diagnostics.
- ffmpeg is never downloaded; it comes from the system or the snap/Flatpak runtime.
- The snap is built by GitHub Actions only, never locally.
- Release checklist: replace `— unreleased` in the `## [x.y.z]` changelog heading with the date,
  keep the metainfo `<release>` in sync, bump `project(VERSION)`.
