#!/usr/bin/env bash
# Sourced by dev-build.sh / dev-run.sh. Provides:
#   RED_RT              runtime content snap root (kf6-core24)
#   RED_RT_LIB_PATH     LD_LIBRARY_PATH value for running dev binaries
#   red_prepare_runtime_farm <build-dir>
#   red_export_runtime_env  <build-dir>
#
# The dev build links against the kde-qt6-core24-sdk snap and runs against the
# kf6-core24 content snap — the exact libraries the shipped snap uses. Host
# glibc stays authoritative (the content snap carries no libc).

RED_RT=/snap/kf6-core24/current
RED_CORE24_BASE=/snap/core24/current/usr/lib/x86_64-linux-gnu

# Some runtime-snap libs depend on Ubuntu base libraries the host may not
# have (libselinux, liblerc, ...). Farm them from the core24 base snap,
# EXCLUDING the glibc family — the host's newer glibc must stay
# authoritative or the loader chokes on GLIBC_PRIVATE symbols.
red_prepare_runtime_farm() {
    local build="$1"
    local farm="$build/core24-libs"
    mkdir -p "$farm"
    local f b
    for f in "$RED_CORE24_BASE"/*.so*; do
        b="$(basename "$f")"
        case "$b" in
            libc.so*|libm.so*|libmvec.so*|libpthread.so*|libdl.so*|librt.so*|\
            ld-linux*|libresolv.so*|libnsl.so*|libnss_*|libanl.so*|libutil.so*|\
            libBrokenLocale.so*|libthread_db.so*) continue ;;
        esac
        ln -sf "$f" "$farm/$b"
    done
}

red_runtime_lib_path() {
    local build="$1"
    # The pulseaudio subdir carries libpulsecommon, which the runtime's
    # libpulse.so.0 needs but reaches only through a RUNPATH that does not exist
    # off-snap; without it Chromium's audio falls back to ALSA and mic capture
    # fails, so WhatsApp reports a call as unsupported.
    local rt="$RED_RT/usr/lib/x86_64-linux-gnu"
    echo "$rt:$rt/libproxy:$rt/pulseaudio:$build/core24-libs"
}

red_export_runtime_env() {
    local build="$1"
    export LD_LIBRARY_PATH="$(red_runtime_lib_path "$build")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="$RED_RT/usr/lib/x86_64-linux-gnu/qt6/plugins"
    export QTWEBENGINEPROCESS_PATH="$RED_RT/usr/lib/qt6/libexec/QtWebEngineProcess"
    export QTWEBENGINE_RESOURCES_PATH="$RED_RT/usr/share/qt6/resources"
    # This Qt build logs to journald when stderr is not a TTY; for dev runs we
    # want plain stderr (file-capturable).
    # PipeWire (screen share via the portal): the runtime's libpipewire hardcodes
    # the Debian SPA/module paths, which do not exist on non-Debian hosts, so
    # pw_loop_new() fails ("can't make support.system handle") and Chromium
    # segfaults. Point it at the matching plugins the runtime snap ships.
    # GPU: the runtime snap ships only stub Mesa (its DRI dir lacks the host GPU
    # driver), so GBM buffer allocation fails and stopping a PipeWire screencast
    # tears down the EGL context and crashes (EGL_BAD_DISPLAY). The shipped snap
    # gets real Mesa from the mesa-2404 content snap via the kde-neon-6
    # extension; wire the same provider here (mirrors gpu-2404-provider-wrapper).
    local mesa=/snap/mesa-2404/current
    if [ -d "$mesa/usr/lib/x86_64-linux-gnu/dri" ]; then
        export LD_LIBRARY_PATH="$mesa/usr/lib/x86_64-linux-gnu:$mesa/usr/lib/x86_64-linux-gnu/dri:$mesa/usr/lib/x86_64-linux-gnu/vdpau:$LD_LIBRARY_PATH"
        export LIBGL_DRIVERS_PATH="$mesa/usr/lib/x86_64-linux-gnu/dri"
        export GBM_BACKENDS_PATH="$mesa/usr/lib/x86_64-linux-gnu/gbm"
        export LIBVA_DRIVERS_PATH="$mesa/usr/lib/x86_64-linux-gnu/dri"
        export __EGL_VENDOR_LIBRARY_DIRS="$mesa/usr/share/glvnd/egl_vendor.d"
        export DRIRC_CONFIGDIR="$mesa/drirc.d"
    fi
    export SPA_PLUGIN_DIR="$RED_RT/usr/lib/x86_64-linux-gnu/spa-0.2"
    export PIPEWIRE_MODULE_DIR="$RED_RT/usr/lib/x86_64-linux-gnu/pipewire-0.3"
    export QT_FORCE_STDERR_LOGGING=1
}
