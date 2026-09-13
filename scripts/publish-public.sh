#!/bin/bash
# Refreshes the public repository (github.com/keshavbhatt/red: README, screenshots,
# changelog, license, icon — never the source) from this tree.
#
#   scripts/publish-public.sh <path-to-public-checkout>
#
# Review and push the resulting commit yourself.
set -euo pipefail
DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:?path to the public checkout}"
[ -d "$DEST/.git" ] || { echo "$DEST is not a git checkout" >&2; exit 1; }

# Red 9 leftovers: the old recipe and launcher no longer build anything.
git -C "$DEST" rm -rqf --ignore-unmatch images snap snap_launcher icon.png

mkdir -p "$DEST/screenshots"
cp "$DIR/packaging/public/README.md" "$DEST/README.md"
cp "$DIR/packaging/public/LICENSE" "$DEST/LICENSE"
cp "$DIR/CHANGELOG.md" "$DEST/CHANGELOG.md"
cp -r "$DIR/screenshots"/. "$DEST/screenshots/"
cp "$DIR/src/resources/icons/hicolor/512x512/apps/com.ktechpit.red.png" "$DEST/icon.png"

git -C "$DEST" add -A
git -C "$DEST" status --short
