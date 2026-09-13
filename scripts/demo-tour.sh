#!/bin/bash
# Drives a running (or freshly started) Red through a short tour for a screen
# recording: home page, a video, a download, TV mode, settings, quit.
# Every step is a command Red's single instance accepts, so nothing has to be
# clicked. Start the recorder first, then:
#
#   scripts/demo-tour.sh                    # the dev build (scripts/dev-run.sh)
#   RED=com.ktechpit.red scripts/demo-tour.sh   # the Flatpak
#   RED=red-app scripts/demo-tour.sh        # the snap
#
# RED is the command that launches Red; extra arguments are forwarded to the
# running instance by Red itself.
set -euo pipefail
DIR="$(cd "$(dirname "$0")/.." && pwd)"
RED="${RED:-}"
VIDEO="${VIDEO:-https://www.youtube.com/watch?v=KLuTLF3x9sA}"   # 4K nature film
DOWNLOAD="${DOWNLOAD:-https://www.youtube.com/watch?v=aqz-KE-bpKQ}" # Big Buck Bunny, short

red() {
    case "$RED" in
        "") "$DIR/scripts/dev-run.sh" -- "$@" ;;
        com.ktechpit.red) flatpak run com.ktechpit.red "$@" ;;
        *) "$RED" "$@" ;;
    esac
}

pause() { sleep "$1"; }

echo "1/5 start on the home page";  red & pause 12
echo "2/5 open a video";            red "$VIDEO" & pause 18
echo "3/5 queue a download";        red --download "$DOWNLOAD" & pause 16
echo "4/5 TV mode";                 red --tv & pause 14
echo "5/5 settings, then quit";     red --settings & pause 8
red --quit
wait
echo "tour finished"
