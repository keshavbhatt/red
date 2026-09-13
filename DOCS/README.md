# DOCS — how this project is tracked

Everything about the Red rewrite lives here. If it is not in `DOCS/`, it did not happen.
`DOCS/` is git-ignored (owner preference, as in whatsie): it is the working notebook, not
the published documentation.

| File | What it is |
|---|---|
| [`FEATURES.md`](FEATURES.md) | Feature list with the decision per row. **The scope contract.** |
| [`ROADMAP.md`](ROADMAP.md) | Milestones with exit criteria. |
| [`PROGRESS.md`](PROGRESS.md) | Session log + milestone status table. Newest first. |
| [`CODING_STANDARDS.md`](CODING_STANDARDS.md) | Binding Qt 6 / C++20 rules (inherited from new-whatsie, adapted). |
| [`DECISIONS.md`](DECISIONS.md) | Architecture Decision Records (append-only). |
| [`LESSONS.md`](LESSONS.md) | Mistakes in Red v9 / TV-mode research / whatsie and the rule each became. |
| [`DESIGN.md`](DESIGN.md) | The UI/UX design: brand, tokens, layout, every screen. |
| `reference/analysis-red-v9.md` | Full technical audit of the original Red (frozen). |

## Workflow

1. **Before coding a feature:** its row in `FEATURES.md` must say `KEEP`.
2. **While coding:** follow `CODING_STANDARDS.md`; new architectural choices get an ADR.
3. **After coding:** update the row's `Status`, add a `PROGRESS.md` entry, run
   `scripts/dev-build.sh --tests`.
4. **Commit style:** conventional commits, no trailers.

## Build & run (dev)

```sh
sudo snap install kde-qt6-core24-sdk kf6-core24   # once
scripts/dev-build.sh --tests
scripts/dev-run.sh                                # or: scripts/dev-run.sh -- --tv
QT_LOGGING_RULES="red.*.debug=true" scripts/dev-run.sh
```
