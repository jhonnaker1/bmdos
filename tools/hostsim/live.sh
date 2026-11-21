#!/bin/bash
#
# Runs BMDOS in the host simulator in real time, shown in a web page:
#
#   tools/hostsim/live.sh [GAMES_DIRECTORY] [PORT]
#
# then open http://127.0.0.1:PORT/ (default port 8064). The simulator's SD
# card is tools/hostsim/out/live, set up as the Pi's: the test programs'
# ZIP (dos/make_test_zip.py) in its dos folder, and the ZIPs, programs and
# disk images from GAMES_DIRECTORY too. Choose one with F12, Run a Game or
# Program. Choosing Reboot (or Shut Down) in the menu starts it again.
# Ctrl+C stops it. (run_tests.sh empties tools/hostsim/out.)
#
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
GAMES=${1:-}
PORT=${2:-8064}
CARD=$HERE/out/live

make -s -C "$HERE" -j4 || exit 1

mkdir -p "$CARD/dos"
python3 "$HERE/dos/make_test_zip.py" "$CARD/dos/bmdostest.zip" || exit 1
if [ -n "$GAMES" ]; then
    find "$GAMES" -maxdepth 1 -type f \( -iname '*.zip' -o -iname '*.dosz' -o -iname '*.exe' \
        -o -iname '*.com' -o -iname '*.img' -o -iname '*.ima' -o -iname '*.vhd' -o -iname '*.iso' \
        -o -iname '*.cue' -o -iname '*.bin' -o -iname '*.chd' \) -exec cp {} "$CARD/dos/" \;
fi

cd "$CARD" || exit 1
while true; do
    BMDOS_LIVE=$PORT BMDOS_CMDLINE="root=$CARD ${BMDOS_CMDLINE:-}" \
        "$HERE/build/bmdos_sim" < /dev/null
    status=$?
    [ $status -ne 0 ] && exit $status
    echo "[live] restarting"
    sleep 1
done
