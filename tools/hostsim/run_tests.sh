#!/bin/bash
#
# Runs the emulator port on the PC (see host_sim.c) through a set of
# scripted tests and checks the results.
#
#   tools/hostsim/run_tests.sh
#   BMDOS_GAME=game.zip tools/hostsim/run_tests.sh   (also starts a game)
#
# No DOS or game files are needed: DOSBox Pure has its own DOS, and the
# tests' programs are made by dos/make_test_zip.py (BEEP.COM sounds the PC
# speaker until a key is pressed, JOY.COM waits for the joystick's
# button). BMDOS_GAME, if set, is a game's ZIP that is started too, and
# its screen saved (out/game/shot.ppm). Output, including screenshots
# (.ppm), goes to tools/hostsim/out/.
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
SIM=$HERE/build/bmdos_sim

make -s -C "$HERE" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" || exit 1

rm -rf "$OUT"
mkdir -p "$OUT/zips"
python3 "$HERE/dos/make_test_zip.py" "$OUT/zips/bmdostest.zip" || exit 1

FAILED=0
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1: $2"; FAILED=$((FAILED + 1)); }

# run NAME CMDLINE SCRIPT: runs the simulator in out/NAME, a stand-in SD
# card with the test ZIP in its dos folder. At most two minutes.
run() {
    local name=$1 cmdline=$2 script=$3
    mkdir -p "$OUT/$name/dos"
    cp "$OUT/zips/bmdostest.zip" "$OUT/$name/dos/"
    (cd "$OUT/$name" && printf "$script" | BMDOS_CMDLINE="root=. $cmdline" "$SIM" > log.txt 2>&1 &
     pid=$!; (sleep 120; kill $pid 2>/dev/null) & killer=$!; wait $pid; kill $killer 2>/dev/null)
}

# check_log NAME REGEX MESSAGE: the log must contain REGEX
check_log() {
    if grep -Eq "$2" "$OUT/$1/log.txt"; then
        pass "$1: $3"
    else
        fail "$1" "$3 (see out/$1/log.txt)"
    fi
}

# check_no_log NAME REGEX MESSAGE: the log must not contain REGEX
check_no_log() {
    if grep -Eq "$2" "$OUT/$1/log.txt"; then
        fail "$1" "$3 (see out/$1/log.txt)"
    else
        pass "$1: $3"
    fi
}

# check_cfg NAME REGEX MESSAGE: out/NAME/bmdos.cfg must have a line REGEX
check_cfg() {
    if grep -Eq "$2" "$OUT/$1/bmdos.cfg" 2>/dev/null; then
        pass "$1: $3"
    else
        fail "$1" "$3 (not in out/$1/bmdos.cfg)"
    fi
}

# count NAME FILE R G B: how many pixels of that colour the screenshot has
count() {
    python3 - "$OUT/$1/$2.ppm" "$3" "$4" "$5" <<'EOF'
import sys
try:
    data = open(sys.argv[1], 'rb').read()
except OSError:
    print(0)
    sys.exit(0)
parts = data.split(b'\n', 3)
px = parts[3]
want = bytes(int(v) for v in sys.argv[2:5])
print(sum(1 for i in range(0, len(px), 3) if px[i:i + 3] == want))
EOF
}

differ() {
    if [ -f "$OUT/$1/$2.ppm" ] && [ -f "$OUT/$1/$3.ppm" ] && ! cmp -s "$OUT/$1/$2.ppm" "$OUT/$1/$3.ppm"; then
        pass "$1: $2 and $3 differ"
    else
        fail "$1" "$2 and $3 should differ"
    fi
}

# sound NAME N: the Nth "audio" line's frames of tone (sound that swings:
# not silence, nor the level a PC speaker is left at once it stops)
sound() {
    grep "audio written" "$OUT/$1/log.txt" | sed -n "${2}p" | sed 's/.*tone \([0-9]*\) frames.*/\1/'
}

# USB HID usages used below.
F12=0x45 DOWN=0x51 ENTER=0x28 SPACE=0x2c POWER=0x66
# KEY USAGE FRAME: press at FRAME, release two frames later
key() { printf '%d key %s 1\\n%d key %s 0\\n' "$2" "$1" $(($2 + 2)) "$1"; }

ZIP=./dos/bmdostest.zip
# HOST_PAD_* bits (src/host.h)
PAD_A=1 PAD_DOWN=0x2000

echo "No content: DOSBox Pure's start menu, then its command line"
run boot "" "250 shot menu.ppm\n$(key $ENTER 260)400 shot prompt.ppm\n401 quit\n"
check_log boot "starting with no content" "starts with nothing to run"
check_log boot "DOSBox Pure running: 640x400, 70.09 Hz" "DOSBox Pure runs (VGA's 70 Hz text mode)"
check_log boot "Program: PUREMENU" "its start menu"
check_log boot "Program: DOSBOX" "Enter: its command line"
check_log boot "THROTTLE.*VSYNC 60" "told it's paced by a 60 Hz display"
differ boot menu prompt
check_no_log boot "audio underrun \\(" "no sound underruns"
if [ -d "$OUT/boot/dos/system" ] && [ -d "$OUT/boot/dos/saves" ]; then
    pass "boot: dos/system and dos/saves made on the card"
else
    fail "boot" "no dos/system or dos/saves on the card"
fi

echo "A ZIP: its programs, the keyboard at the DOS prompt, the PC speaker"
# The start menu lists BEEP.COM, JOY.COM, SERIAL.COM, then "Go to Command
# Line".
run zip "content=$ZIP" "$(key $DOWN 200)$(key $DOWN 205)$(key $DOWN 207)$(key $ENTER 210)300 type BEEP\n$(key $ENTER 330)380 audio\n420 audio\n$(key $SPACE 430)480 audio\n520 audio\n530 quit\n"
check_log zip "starting \./dos/bmdostest\.zip" "the ZIP is the PC's content"
check_log zip "Program: BEEP" "BEEP typed at the prompt runs BEEP.COM"
before=$(sound zip 1) during=$(sound zip 2) after1=$(sound zip 3) after2=$(sound zip 4)
if [ -n "$during" ] && [ $((during - before)) -ge 20000 ]; then
    pass "zip: the speaker sounds ($((during - before)) frames of tone in 40)"
else
    fail "zip" "no speaker sound (${before:-?} to ${during:-?})"
fi
if [ -n "$after2" ] && [ "$after2" -eq "$after1" ]; then
    pass "zip: a key stops it"
else
    fail "zip" "the sound went on after the key (${after1:-?} to ${after2:-?})"
fi

echo "A gamepad: as keys (DOSBox Pure's mapper), and as the PC's joystick"
# Auto: an unknown program's gamepad is the keyboard (D-pad arrows, A
# Enter): it chooses JOY.COM in the start menu.
run padkeys "content=$ZIP" "200 pad 0 $PAD_DOWN\n206 pad 0 0\n220 pad 0 $PAD_A\n226 pad 0 0\n300 quit\n"
check_log padkeys "Program: JOY" "the D-pad and A chose and ran JOY.COM"
# PC Joystick 1: A is the joystick's first button, which JOY.COM waits for.
run joystick "content=$ZIP pad1_type=joystick1" "$(key $DOWN 200)$(key $ENTER 210)380 shot waiting.ppm\n400 pad 0 $PAD_A\n440 pad 0 0\n460 shot pressed.ppm\n461 quit\n"
check_log joystick "Program: JOY" "JOY.COM runs"
if [ "$(grep -c "Program: DOSBOX" "$OUT/joystick/log.txt")" -ge 2 ]; then
    pass "joystick: the button ended JOY.COM"
else
    fail "joystick" "JOY.COM never saw the button"
fi
differ joystick waiting pressed

echo "The menu: settings saved in bmdos.cfg"
# Gamepads (the sixth item), Gamepad 1, PC Joystick 1 (the third type).
run padmenu "content=$ZIP" "$(key $F12 200)$(key $DOWN 205)$(key $DOWN 210)$(key $DOWN 215)$(key $DOWN 220)$(key $DOWN 225)$(key $ENTER 230)$(key $ENTER 240)$(key $DOWN 250)$(key $DOWN 255)$(key $ENTER 260)300 quit\n"
check_cfg padmenu "^pad1_type=joystick1" "Gamepads > Gamepad 1 > PC Joystick 1"
# DOSBox Settings (the fifth item), its first category, its first
# setting, the next value.
run coremenu "content=$ZIP" "$(key $F12 200)230 shot menu.ppm\n$(key $DOWN 235)$(key $DOWN 240)$(key $DOWN 245)$(key $DOWN 250)$(key $ENTER 260)290 shot settings.ppm\n$(key $ENTER 300)$(key $ENTER 310)$(key $DOWN 320)$(key $ENTER 330)$(key $F12 340)400 quit\n"
check_cfg coremenu "^dosbox_pure_[a-z0-9_]+=" "a DOSBox Pure setting changed in the menu"
check_log coremenu "DOSBox Pure settings in [0-9]+ categories" "the core's settings were listed"
differ coremenu menu settings
n=$(count coremenu menu 0 0 172)
if [ "$n" -gt 100000 ]; then
    pass "coremenu: the menu is on the screen, blue ($n pixels)"
else
    fail "coremenu" "the menu isn't on the screen ($n blue pixels)"
fi
# DOSBox Pure's Menu and Keyboard (the fourth item): its on-screen menu
# over the DOS prompt.
run coremenu2 "" "$(key $ENTER 200)290 shot before.ppm\n$(key $F12 300)$(key $DOWN 310)$(key $DOWN 315)$(key $DOWN 320)$(key $ENTER 330)400 shot after.ppm\n401 quit\n"
differ coremenu2 before after
# Restart the PC (the third item): the core starts again.
run restart "content=$ZIP" "$(key $F12 200)$(key $DOWN 210)$(key $DOWN 215)$(key $ENTER 220)400 quit\n"
check_log restart "starting the PC again" "Restart the PC"
if [ "$(grep -c "DOSBox Pure running" "$OUT/restart/log.txt")" -ge 2 ]; then
    pass "restart: DOSBox Pure ran again"
else
    fail "restart" "DOSBox Pure didn't start again"
fi

echo "The web UI's Run, and content that isn't there"
run webrun "" "200 run $ZIP\n500 quit\n"
check_log webrun "web UI: starting \./dos/bmdostest\.zip" "the web UI's request"
check_log webrun "starting \./dos/bmdostest\.zip" "the PC starts with it"
check_cfg webrun "^content=\./dos/bmdostest\.zip" "kept as the content to start with"
run missing "content=./dos/nothere.zip" "200 quit\n"
check_log missing "nothere\.zip is not there: starting without it" "says so and starts without it"

echo "Sound at the Pi 4 and 5's HDMI rate (48 kHz)"
mkdir -p "$OUT/rate48"
(cd "$OUT/rate48" && printf '200 quit\n' | BMDOS_SIM_RATE=48000 BMDOS_CMDLINE="root=." "$SIM" > log.txt 2>&1)
check_log rate48 "DOSBox Pure running: .* sound 48000 Hz" "the core makes its sound at 48 kHz"

echo "A display without 32-bit colour (RGB565)"
mkdir -p "$OUT/rgb565"
(cd "$OUT/rgb565" && printf '250 shot menu.ppm\n251 quit\n' | BMDOS_SIM_NO32=1 BMDOS_CMDLINE="root=." "$SIM" > log.txt 2>&1)
if [ -f "$OUT/rgb565/menu.ppm" ] && [ "$(count rgb565 menu 0 0 0)" -lt 200000 ]; then
    pass "rgb565: the start menu drawn in RGB565"
else
    fail "rgb565" "no picture in RGB565"
fi

echo "A USB stick with the content on it, pulled out"
mkdir -p "$OUT/usb/usb"
cp "$OUT/zips/bmdostest.zip" "$OUT/usb/usb/"
(cd "$OUT/usb" && printf '200 usb 1 0\n400 quit\n' | BMDOS_USB=1 \
    BMDOS_CMDLINE="root=. content=usb/bmdostest.zip" "$SIM" > log.txt 2>&1)
check_log usb "starting usb/bmdostest\.zip" "started from the stick"
check_log usb "USB stick 1 out: the PC's content was on it" "the stick pulled out is noticed"
check_log usb "usb/bmdostest\.zip was on the USB stick pulled out: starting without it" "the PC starts again without it"

echo "FujiNet: COM1 to a FujiNet-PC (a stand-in) over TCP"
# fake_fujinet.py listens as FujiNet-PC's BoIP does (BMDOS connects to it),
# and answers each byte with its case flipped; SERIAL.COM sends BMDOS out of COM1 and ends
# once five bytes have come back.
FN_PORT=$((47000 + $$ % 1000))
mkdir -p "$OUT/fujinet/dos"
cp "$OUT/zips/bmdostest.zip" "$OUT/fujinet/dos/"
python3 "$HERE/fake_fujinet.py" $FN_PORT 60 > "$OUT/fujinet/fake.txt" 2>&1 &
fake=$!
sleep 0.5
(cd "$OUT/fujinet" && printf "$(key $DOWN 200)$(key $DOWN 205)$(key $DOWN 210)$(key $ENTER 220)300 type serial\n$(key $ENTER 400)1200 quit\n" | \
    BMDOS_CMDLINE="root=. content=./dos/bmdostest.zip fujinet=1 fujinet_host=127.0.0.1 fujinet_port=$FN_PORT" \
    "$SIM" > log.txt 2>&1)
sleep 1
kill $fake 2>/dev/null
check_log fujinet "FujiNet on COM1" "the FujiNet port is COM1"
check_log fujinet "FujiNet-PC connected \(127\.0\.0\.1:[0-9]+\), on COM1" "connected to FujiNet-PC"
check_log fujinet "Program: SERIAL" "SERIAL.COM runs"
if grep -q "got b'BMDOS'" "$OUT/fujinet/fake.txt"; then
    pass "fujinet: COM1's bytes reached FujiNet-PC"
else
    fail "fujinet" "FujiNet-PC got no BMDOS (see out/fujinet/fake.txt)"
fi
if [ "$(grep -c "Program: DOSBOX" "$OUT/fujinet/log.txt")" -ge 2 ]; then
    pass "fujinet: its answers came back in, and SERIAL.COM ended"
else
    fail "fujinet" "SERIAL.COM never got its five bytes back"
fi

echo "Wi-Fi settings changed in the menu keep the Wi-Fi key (from BMCoco)"
# A key of 64 hex digits (as wpa_passphrase writes it, not in quotes):
# Network (the ninth item), Wi-Fi Country (four down), GB.
KEY=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
mkdir -p "$OUT/wifikey"
printf 'country=US\n\nnetwork={\n\tssid="Home"\n\tpsk=%s\n\tkey_mgmt=WPA-PSK\n}\n' $KEY > "$OUT/wifikey/wpa_supplicant.conf"
(cd "$OUT/wifikey" && printf "$(key $F12 200)$(for i in 1 2 3 4 5 6 7 8; do key $DOWN $((205 + i * 5)); done)$(key $ENTER 260)$(key $DOWN 270)$(key $DOWN 275)$(key $DOWN 280)$(key $DOWN 285)$(key $ENTER 290)$(key 0x2a 300)$(key 0x2a 305)310 type GB\n$(key $ENTER 330)400 quit\n" | \
    BMDOS_CMDLINE="root=." "$SIM" > log.txt 2>&1)
if grep -q "^country=GB" "$OUT/wifikey/wpa_supplicant.conf"; then
    pass "wifikey: the country changed"
else
    fail "wifikey" "the country didn't change (see out/wifikey/wpa_supplicant.conf)"
fi
if grep -q "psk=$KEY\$" "$OUT/wifikey/wpa_supplicant.conf"; then
    pass "wifikey: the 64 digit key kept as it was"
else
    fail "wifikey" "the key was spoiled (see out/wifikey/wpa_supplicant.conf)"
fi

echo "Web UI requests and the power key"
run reboot "" '200 webui reboot\n260 quit\n'
check_log reboot "reboot requested" "reboots"
run coldstart "" '200 webui coldstart\n400 quit\n'
check_log coldstart "web UI: restart" "restarts the PC"
run powerkey "" "$(key $POWER 200)260 quit\n"
check_log powerkey "power off requested" "the power key powers off"

if [ -n "${BMDOS_GAME:-}" ]; then
    echo "A game: $BMDOS_GAME"
    mkdir -p "$OUT/game/dos"
    cp "$BMDOS_GAME" "$OUT/game/dos/game.zip"
    (cd "$OUT/game" && printf "$(key $ENTER 200)900 shot shot\n901 audio\n902 quit\n" | \
        BMDOS_CMDLINE="root=. content=./dos/game.zip" "$SIM" > log.txt 2>&1)
    check_log game "starting \./dos/game\.zip" "the game's ZIP is the content"
    check_no_log game "audio underrun \\(" "no sound underruns"
    echo "      (its screen: out/game/shot.ppm)"
fi

echo
if [ $FAILED -eq 0 ]; then
    echo "All tests passed."
else
    echo "$FAILED test(s) failed."
fi
exit $((FAILED > 0))
