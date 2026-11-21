# Build Instructions

BMDOS builds on Linux (Debian, Ubuntu, WSL) and macOS.

## Prerequisites

On Debian or Ubuntu:

```
sudo apt-get install build-essential git python3 curl xz-utils
```

On macOS (Apple silicon or Intel), with the Xcode command line tools and
[Homebrew](https://brew.sh):

```
xcode-select --install
brew install bash gnu-getopt
```

circle-stdlib's `configure` needs bash 4 or later and GNU `getopt`, which
`make_all.sh` puts first on the PATH on macOS. newlib's build would like
`makeinfo` (texinfo) for its manuals and Circle's Wi-Fi firmware download
uses `wget`; without them, `make_all.sh` and `make_wifi_firmware.sh` use
stand-ins (the manuals aren't needed, and `curl` does the download).

The Arm GNU Toolchain 15.2.rel1 is downloaded into
`~/.cache/bmdos/toolchains` the first time a build needs it
(`get_gnu_toolchain.sh`), built for your computer (Linux or macOS):
`aarch64-none-elf` for the 64-bit Pi 4 and 5 kernels (and `arm-none-eabi`
for the family's 32-bit Pi 2 and 3 kernels, which BMDOS doesn't ship). If it can't be downloaded,
`BMDOS_ALLOW_ANY_TOOLCHAIN=1` lets the build use whatever
`arm-none-eabi-gcc` (or `aarch64-none-elf-gcc`) is on the PATH.

## Downloading the files

```
git clone --recursive https://github.com/jhonnaker1/bmdos.git
```

or, in an existing clone, `git submodule update --init --recursive`.
DOSBox Pure is a submodule (`third_party/dosbox-pure`, from Codeberg).

## Building an SD card

```
./build_sdcard.sh
```

builds everything from clean and stages the boot partition in
`build/sdcard`, with a kernel for each board: `kernel8-rpi4.img` (with
`armstub8-rpi4.bin`) for the Pi 4 B and Pi 400, and `kernel_2712.img` for
the Pi 5, Pi 500 and Pi 500+. `./build_sdcard.sh pi4` or
`./build_sdcard.sh pi5` builds just one. Copy the contents of that folder
to the root of a FAT32 formatted SD card.

The PC tests (`tools/hostsim`, `tools/pi4kms` and `tools/pi4scaler`, below)
run first, and the build stops if any of them fails; `--skip-tests` leaves
them out. They need nothing that isn't part of BMDOS.

While developing, `./build_sdcard.sh --skip-patches` keeps the third party
libraries already built and only rebuilds the kernel, which takes seconds.
The libraries are built for one board at a time, so without a board it
rebuilds the kernel for the board built last; a board they aren't built for
is built from clean. With
[networking](USER_GUIDE.md#networking-and-the-web-ui) on, a new kernel can
then be uploaded to the Pi from the web UI and the Pi
rebooted from there, without touching the SD card.

## The individual steps

1. `./clean_all.sh` cleans everything, including the third party builds,
   and resets Circle to its pinned revision.
2. `./make_all.sh pi4|pi5 [--skip-patches]` applies `src/patches/` to
   Circle, builds circle-stdlib (Circle and newlib) and the Circle addons
   for one board, then the kernel: `kernel8-rpi4.img` (64-bit) for pi4,
   which also builds Circle's `armstub8-rpi4.bin`, or `kernel_2712.img`
   (64-bit) for pi5. (pi2 and pi3 may build 32-bit kernels, as in the rest
   of the family, without DOSBox's dynamic recompiler: untried.) It only
   needs running again after
   `clean_all.sh` or a change to the patches or circle-stdlib. To change
   board, run `clean_all.sh` first (make_all.sh refuses otherwise).
3. `BMDOS_AARCH=64 source get_gnu_toolchain.sh && make` rebuilds just the
   kernel.
4. `./make_wifi_firmware.sh DIR` puts the Wi-Fi firmware in `DIR/firmware`
   (`build_sdcard.sh` does this).

`config.txt` has a section per board that picks its kernel. The emulator
runs on core 1. See [PI4.md](PI4.md) and [PI5.md](PI5.md) for the Pi 4
and 5 builds.

## Testing on a PC

`tools/hostsim` builds the emulator port (DOSBox Pure plus `src/port/`,
with the Pi's cooperative threads) for the PC, with a simulated display,
sound and input in place of the Raspberry Pi. Its scripted tests need no
DOS or games: DOSBox Pure's start menu and DOS prompt, two tiny test
programs in a ZIP made by `tools/hostsim/dos/make_test_zip.py` (the PC
speaker, a joystick button), the keyboard, a gamepad as keys and as a PC
joystick, the menu and the settings it saves, the web UI's requests, USB
sticks and the power key. `BMDOS_GAME=game.zip` also starts a game of
your own.

```
tools/hostsim/run_tests.sh
```

Screenshots (`.ppm`) are written to `tools/hostsim/out/`.

To play with the port on the PC, run it live:

```
tools/hostsim/live.sh [/path/to/your/games]
```

and open http://127.0.0.1:8064/ in a browser: the screen, keyboard, mouse
and sound are in the page. The simulator's SD card is
`tools/hostsim/out/live`, set up as the Pi's. This runs BMDOS's port and
DOSBox Pure on the PC (without the dynamic recompiler on a Mac), not the
Pi kernel (no emulator
shows the Pi's VideoCore display). The web page is served on 127.0.0.1
only.

The simulator can also be driven by a script of key presses, mouse moves
and screenshots; see the top of `tools/hostsim/host_sim.c` for the
commands. [SIMULATOR.md](SIMULATOR.md) has more: setting it up, debugging
it with gdb, lldb or strace, and getting a log from the Pi.

`tools/pi4scaler` tests the shared Pi 4/5 ARM smooth scaling and CRT shader
(`src/av/scaler.*`): every pixel is checked against crt-pi worked out in
floating point, and the pictures are written to `tools/pi4scaler/out/`.
`tools/pi4kms` tests the Pi 4/5 direct display drivers (`src/kms/`, see
[PI4.md](PI4.md) and [PI5.md](PI5.md)): shared filter kernels, EDID, mode
choice, HDMI packets and audio clocks, and each board's display lists and
PHY settings.
Both come from BMA800XL. `tools/pi4scaler` also tests where the picture
goes on the display (`src/av/layout.h`, every Pi, from BMMacintosh): every
picture on 16:9, 16:10 and 5:4 displays fits, is centred and keeps its
shape.

```
tools/pi4scaler/run_tests.sh
tools/pi4kms/run_tests.sh
```

`tools/qemu/run_qemu.sh` starts a Pi 4 kernel in QEMU's `raspi4b` (`brew
install qemu`) and prints its log from the serial port, for a kernel that
stops before its log reaches the SD card. QEMU covers the start only: it
has no SD card as Circle drives it, no display hardware, USB or network.

```
tools/qemu/run_qemu.sh build/sdcard/kernel8-rpi4.img
```

## The web UI

The web UI's pages are in `src/webui/assets/`. They are compiled into the
kernel through `src/webui/webui_assets.c`, which the Makefile regenerates
with `tools/gen_webui_assets.py` whenever an asset changes.

## Layout

| Path                         | What                                        |
|------------------------------|---------------------------------------------|
| `src/main.cpp`, `kernel.*`   | Circle kernel; USB, GPIO, SD card, cores    |
| `src/network.*`              | Ethernet / Wi-Fi start up (from BMC64)      |
| `src/network_time_sync.*`    | Clock from the network (from BMC64)         |
| `src/webui/`                 | Web UI server, file manager and pages (from BMC64) |
| `src/sdcard/`                | SD card helpers used by the web UI (from BMC64) |
| `src/fbl.*`, `crt_pi_*`      | Display layer and CRT shaders (from BMC64)  |
| `src/vchiqsound*`            | Sound through the VideoCore (from BMC64)    |
| `src/options.*`              | `cmdline.txt` options                       |
| `src/host.h`                 | Interface between kernel and emulator       |
| `src/port/platform.c`        | The libretro frontend: runs DOSBox Pure a frame at a time; its picture, sound, input; content; web UI requests |
| `src/port/coreopts.c`        | DOSBox Pure's settings (core options), kept in bmdos.cfg |
| `src/port/vfs.c`             | The libretro file interface (VFS v3) over the C library |
| `src/port/keys.c`            | USB keys to libretro's RETROK codes          |
| `src/port/clock.c`           | The core's clock and sleeps                  |
| `src/port/circle/`           | Cooperative pthreads on libco, POSIX functions and headers newlib lacks |
| `src/port/menu.c`, `osd.c`   | The menu and its on-screen display          |
| `src/port/fswrap.c`          | `stat()` of the SD card's root, which FatFs can't do (`--wrap=stat`); `readdir()` with `d_type` |
| `src/port/bigalloc.c`        | Big blocks of memory used again once freed (`--wrap=malloc`, `--wrap=free`) |
| `src/port/dosbox.mk`         | DOSBox Pure's sources and flags             |
| `src/patches/`               | Changes applied to Circle by `make_all.sh`  |
| `sdcard/`                    | Files for the SD card                       |
| `third_party/dosbox-pure`    | DOSBox Pure (submodule, from Codeberg)      |
| `third_party/circle-stdlib`  | Circle and newlib (submodule)               |
| `src/av/`                    | Shared Pi 4/5 video, ARM scaling / CRT shader and sound |
| `src/v3d/`                   | Shared Pi 4/5 GPU driver and CRT rendering  |
| `src/kms/common/`            | Shared Pi 4/5 filter kernels and mode calculations |
| `src/kms/pi4/`               | Pi 4 direct display driver and PHY          |
| `src/kms/pi5/`               | Pi 5 direct display driver and PHY          |
| `tools/hostsim`              | PC test harness                             |
| `tools/pi4scaler`            | PC test of shared Pi 4/5 ARM scaling and CRT rendering |
| `tools/pi4kms`               | PC test of Pi 4/5 display driver calculations |
| `tools/qemu`                  | Starts a Pi 4 kernel in QEMU, for start-up crashes |
