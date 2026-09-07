#!/usr/bin/env bash
# Runs the dev build against the kf6-core24 runtime content snap — the very
# libraries the shipped red snap uses. Host fonts, display and D-Bus work
# directly; Chromium's sandbox stays ON.
#
#   scripts/dev-run.sh                 # launch the app
#   scripts/dev-run.sh --ctest [args]  # run the test suite (env baked in by CMake)
#   scripts/dev-run.sh -- --some-flag  # pass arguments to red
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${RED_BUILD_DIR:-$DIR/build}"

# shellcheck source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

[ -d "$RED_RT/usr/lib/x86_64-linux-gnu" ] || {
    echo "kf6-core24 runtime snap missing: sudo snap install kf6-core24" >&2; exit 1; }

if [ "${1:-}" = "--ctest" ]; then
    # ctest is a host binary: keep the host environment. The tests themselves
    # get the runtime env through their CTest ENVIRONMENT property
    # (tests/CMakeLists.txt, from RED_SNAP_RUNTIME*).
    cd "$BUILD"
    exec ctest --output-on-failure "${@:2}"
fi

red_prepare_runtime_farm "$BUILD"
red_export_runtime_env "$BUILD"
# Route file dialogs and file opening through xdg-desktop-portal so they use
# the system dialogs (the KDE platform-theme plugin is not in the runtime snap;
# a host QT_QPA_PLATFORMTHEME like qt5ct would otherwise force Qt's own dialog).
# The shipped snap/flatpak get the native theme from their runtime instead.
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME_OVERRIDE:-xdgdesktopportal}"
# Everything of Red's at debug, minus the page's own console (red.web.js), which
# is chatty; add "red.web.js.debug=true" to QT_LOGGING_RULES when you need it.
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-red.*.debug=true;red.web.js.debug=false}"

[ "${1:-}" = "--" ] && shift
cd "$DIR"
exec "$BUILD/src/red" "$@"
