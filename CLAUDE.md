# Red — project instructions

Red is an open-source YouTube desktop client (Qt 6.11, C++20, CMake). Read `DOCS/` for the
feature contract (FEATURES), design (DESIGN), decisions (DECISIONS, ADRs) and progress log.
The public repository github.com/keshavbhatt/red is the only remote (`master`: Red 10,
`archive-2020`: the public materials from before); `DOCS/` is committed. The licence split is
in LICENSING.md and REUSE.toml: GPL-3.0-or-later, licensing module under its own licence
(ADR-020).

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
- Release checklist: replace the unreleased marker in the `## [x.y.z]` changelog heading with
  ` - YYYY-MM-DD`, keep the metainfo `<release>` in sync, bump `project(VERSION)`, then push the
  tag `vX.Y.Z`. The tag publishes the snap to stable and creates the GitHub release with the
  changelog section as notes (ADR-021); pushes to `master` only reach edge.
