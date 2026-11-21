# Testing and debugging BMDOS on a PC

BMDOS isn't tested in QEMU, beyond its start (`tools/qemu/run_qemu.sh`,
see [BUILDING.md](BUILDING.md)). On the Pi it runs bare metal, on the
Circle kernel with no Linux underneath, and QEMU's Raspberry Pi machines
don't emulate enough of the Pi (its VideoCore display, HDMI sound, USB and
Wi-Fi as BMDOS uses them) to run it. There would be nothing to strace either:
strace follows a Linux program's system calls, and BMDOS makes none.

What BMDOS has instead is a **PC simulator** (`tools/hostsim`, from
BMCoco): the same emulator code that runs on the Pi (DOSBox Pure and
BMDOS's libretro front end for it, `src/port/`, with the Pi's cooperative
threads), built as an ordinary program for Linux or macOS, with a simulated display, sound, keyboard, mouse, joysticks and SD
card in place of the Pi's. Because it's an ordinary program, **gdb, lldb,
strace and the rest work on it** as on any other program.

## What it can and can't show

The simulator runs the PC's emulation, DOS and DOS programs, the menu,
the content handling, and BMDOS's pacing and sound logic. On a Mac it
leaves out DOSBox's dynamic recompiler (Apple's Arm Macs won't run code
written into ordinary memory), which only the Pi's 64-bit kernels have;
a Linux PC has DOSBox's x86 one. A problem in any
of those usually shows up in it too.

It does **not** run the Pi side: the Circle kernel, the Pi's display and
sound drivers, USB, Wi-Fi and Circle's network stack, and the SD card
driver. And a PC is much faster than a Pi: the simulator says nothing about
whether a Pi keeps up (the log's speed line, on the Pi, does).

So the simulator is the first place to look, and if a problem doesn't show
up there, the Pi's log (at the end of this page) is the next.

## Setting it up

You need a C compiler, `make`, `git`, `curl`, `unzip` and Python 3. No
Raspberry Pi toolchain is needed for the simulator.

**Linux** (Debian, Ubuntu, Raspberry Pi OS):

```
sudo apt install build-essential git curl unzip python3 gdb strace
```

**macOS**: install Xcode's command line tools (`xcode-select --install`),
which include `clang`, `make`, `git` and the `lldb` debugger.

Get the source, with DOSBox Pure (a git submodule):

```
git clone https://github.com/jhonnaker1/bmdos
cd bmdos
git submodule update --init third_party/dosbox-pure
```

(`git clone --recursive` works too, but also fetches circle-stdlib, which
only the Pi build needs.)

Build and run the tests (nothing of yours is needed: DOSBox Pure has its
own DOS, and the tests make their own programs):

```
tools/hostsim/run_tests.sh
BMDOS_GAME=/path/to/a/game.zip tools/hostsim/run_tests.sh   # a game too
```

The first build takes a minute (DOSBox Pure is about 250 files). The tests
start DOSBox Pure with nothing and go to its DOS prompt; start a ZIP of two
tiny programs (`tools/hostsim/dos/make_test_zip.py`: BEEP.COM sounds the
PC speaker until a key, JOY.COM waits for the joystick's button) and type
at the prompt; press the gamepad as keys and as a PC joystick; check the
menu and the settings it saves, DOSBox Pure's own menu, restarting, the
web UI's requests, a USB stick pulled out, sound at 48 kHz, an RGB565
display and the power key.

Each test runs in its own folder under `tools/hostsim/out/`, which stands
in for the SD card, with its log (`log.txt`) and screenshots (`.ppm`).

## Using it live

```
tools/hostsim/live.sh [/path/to/your/games]
```

then open <http://127.0.0.1:8064/> in a browser. The PC's screen, sound,
keyboard and mouse are in the page, and F12 opens BMDOS's menu as on the
Pi. The simulator's SD card is `tools/hostsim/out/live`, set up as the
Pi's: the test programs' ZIP and your games (ZIPs, programs, disk images
from the folder given) in `dos`, to choose with F12, Run a Game or
Program.

Options from `cmdline.txt` go in `BMDOS_CMDLINE`:

```
BMDOS_CMDLINE="volume=50" tools/hostsim/live.sh
```

Its messages (the same as the Pi's log, DOSBox Pure's included) go to the
terminal. Ctrl+C stops it.

## Debugging it

### Where is it stuck? (gdb or lldb)

Start it live as above, get it into the state where it's stuck, and attach
a debugger to it from another terminal.

Linux:

```
sudo gdb -p $(pgrep -f build/bmdos_sim)
(gdb) thread apply all bt
```

macOS:

```
lldb -p $(pgrep -f build/bmdos_sim)
(lldb) thread backtrace all
```

The backtrace shows where it is, in BMDOS's and DOSBox Pure's source
(the simulator is built with debugging information). DOSBox runs on a
coroutine of its own (`src/port/circle/threads.c`), so a debugger shows
the frontend's stack (under `retro_run()`) and not DOSBox's while DOSBox
waits for the next frame; break in DOSBox itself (`CPU_Core_Normal_Run`,
say) to see its. Type `continue` to let it go on, or `detach` and `quit` to leave it
running.

To run it under the debugger from the start instead, run it from its SD
card folder:

```
cd tools/hostsim/out/live
BMDOS_LIVE=8064 BMDOS_CMDLINE="root=." gdb ../../build/bmdos_sim
(gdb) run < /dev/null
```

and press Ctrl+C in gdb when it's stuck.

### DOSBox Pure's own messages

DOSBox Pure's messages go to the log as `[core] ...` lines: which program
runs (`Program: ...`), what it couldn't do. Its debug messages are left
out unless `core_debug=1` is in `BMDOS_CMDLINE` (or `cmdline.txt`).

### Scripts

The simulator can also run a script of key presses, mouse moves and
screenshots on its standard input, with time measured in frames, so that a
problem can be repeated exactly. For example:

```
cd tools/hostsim/out/live
printf '250 shot start.ppm\n260 key 0x45 1\n262 key 0x45 0\n290 shot menu.ppm\n291 quit\n' |
    BMDOS_CMDLINE="root=." ../../build/bmdos_sim
```

saves DOSBox Pure's start menu at frame 250, presses F12 (USB HID usage
0x45) and saves BMDOS's menu at frame 290. The commands are listed at the top of
`tools/hostsim/host_sim.c`, and `tools/hostsim/run_tests.sh` has many
examples.

Useful settings, as environment variables:

| Variable                        | Does                                                       |
|---------------------------------|------------------------------------------------------------|
| `BMDOS_CMDLINE`             | Options as in `cmdline.txt` (`root=.` is the SD card)      |
| `BMDOS_REFRESH`             | The display's refresh rate (default 60)                    |
| `BMDOS_SIM_RATE`            | The sound output's rate (48000 for a Pi 4 or 5's HDMI)     |
| `BMDOS_IP`                  | Pretend the network is up with this address (from frame 100) |
| `BMDOS_WIFI`                | Pretend Wi-Fi is running, with a few networks to choose from |
| `BMDOS_REFRESH_LATER`       | Change the display's refresh rate to this at frame 30      |
| `BMDOS_USB`                 | This many USB sticks plugged in from the start (the folders `usb/`, `usb2/`, `usb3/`); the script's `usb 1 1` plugs stick 1 in, `usb 1 0` pulls it out |
| `BMDOS_SIM_HELPER`          | `1`: a helper core (a thread), as on the Pi 5 (BMDOS doesn't use it yet) |
| `BMDOS_SIM_NO32`            | `1`: no 32-bit frame buffer, so the PC's picture is RGB565 |

## On the Pi itself

On the Pi there is no debugger or strace, but there is a log. Add
`enable_logging=file` to the line in `cmdline.txt`, and BMDOS writes
`bmdos.log` in the root of the SD card (and to the serial port: GPIO14
TX and GPIO15 RX at 115200 baud, for a USB serial adapter, which keeps
working even when the SD card doesn't).

What the log shows:

- The Pi, the display and sound, and what the PC runs.
- DOSBox Pure's own messages: which program runs, its settings, what
  went wrong.
- Every 10 seconds, the speed line: frames shown against the display's
  refresh, how much of the time DOSBox took, the ARM's clock and
  temperature, and how full the sound buffer stayed.
- If the emulation stops moving for 3 seconds, `Emulator has not moved for
  3 s; last seen:` and what it was doing. That line is the most useful one
  for a hang.

When something goes wrong, the most helpful report has:

1. What you did, step by step, and what happened.
2. The BMDOS version and which Pi.
3. `bmdos.log` from the card, with `enable_logging=file` (taking the
   card out right after the problem; the log is written as it goes).
4. Your `cmdline.txt` and `bmdos.cfg`.
5. Which game or program, or the file if it's free to share.
