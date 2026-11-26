# BMDOS

BMDOS is a bare metal MS-DOS PC emulator for the Raspberry Pi 4 and 5, for
DOS games. It runs [DOSBox Pure](https://codeberg.org/schelling/dosbox-pure)
directly on the hardware, with no Linux underneath, the same way
[BMC64](https://github.com/randyrossi/bmc64) does for the Commodore 64,
[BMA800XL](https://github.com/aminch/bma800xl) for the Atari 800XL and
[BMAmiga](https://github.com/jhonnaker1/bmamiga) for the Amiga.

BMDOS is based on BMNeXT, which is based on BMAtariST, BMApple2, BMCoco,
[BMA800XL](https://github.com/aminch/bma800xl) by Adam Minchinton, and
[BMC64](https://github.com/randyrossi/bmc64) by Randy Rossi; its libretro
front end comes from BMAmiga. The emulator is DOSBox Pure, by Bernhard
Schelling, built as its libretro core; the kernel, display, sound, network
and web UI come from those projects. What BMDOS took from each is in
[docs/INHERITANCE.md](docs/INHERITANCE.md).

BMDOS wouldn't be possible without the work behind the open source
projects it builds on. Thanks to their authors and contributors for making
it available; see the [third-party](docs/THIRD_PARTY.md) acknowledgements.

**Status:** version 0.1.0 (2026-11-25), the first release. Tried on a
Raspberry Pi 5 and a Pi 4: Commander Keen, Doom, Second Reality, One Must
Fall 2097, Duke Nukem 3D and the 3dfx Tomb Raider demo, MS-DOS 6.22 and
FreeDOS from disk images, and CD images with CD audio. 3D games run at or
near full speed on the Pi 5 and somewhat slower on the Pi 4. USB sticks,
gamepads, FujiNet and the web UI have so far been tried only in the PC
simulator (`tools/hostsim`). See the [changelog](CHANGELOG.md).

- DOS games from a ZIP, as they are downloaded: DOSBox Pure lists the
  programs in it and starts the one you choose (and can remember it)
- DOS programs (.exe, .com, .bat), CD images (.iso, .cue, .chd), floppy
  and hard disk images (.img, .ima, .vhd)
- DOSBox Pure's own DOS: no DOS needed. Windows 9x or MS-DOS hard disk
  images of your own can be booted too
- Sound Blaster, Gravis Ultrasound, AdLib, PC speaker, Tandy and Disney
  sound, General MIDI with a sound font, as DOSBox Pure has them; CD
  audio from CUE/BIN images
- The 3dfx Voodoo, in software, its triangles drawn on three of the Pi's
  cores (the Tomb Raider demo runs at full speed on a Pi 5)
- USB keyboards and mice, and USB gamepads: DOSBox Pure maps the gamepad
  for the games it knows, to keys otherwise, or makes it a PC joystick
- DOSBox Pure's on-screen menu and keyboard, and its gamepad mapper
- A menu (F12) for what to run, all of DOSBox Pure's settings, the
  gamepads, video, sound and network
- Optional Ethernet or Wi-Fi with BMC64's web UI: manage the SD card from a
  browser and start games
- Games on USB sticks (up to three, FAT32 or exFAT) as well as the SD card
- FujiNet: a FujiNet-PC on the network as the PC's serial port (see the
  User Guide)
- No shutdown needed: just switch it off

## Games

None come with BMDOS: copy your own (or freeware and shareware games) into
the card's `dos` folder. A game's ZIP works as it is.

## Supported hardware

The **Raspberry Pi 4 Model B and Pi 400** (`kernel8-rpi4.img`, 64-bit) and
the **Raspberry Pi 5, Pi 500 and Pi 500+** (`kernel_2712.img`, 64-bit),
with DOSBox's dynamic recompiler for the ARM's 64-bit instructions. See
[docs/PI4.md](docs/PI4.md) and [docs/PI5.md](docs/PI5.md).

## Getting started

1. Format an SD card as FAT32.
2. Copy the contents of a release zip (or of `build/sdcard/` after building)
   to the root of the card.
3. Copy a game's ZIP into `dos`.
4. Boot the Pi with a USB keyboard connected. DOSBox Pure's start menu
   comes up. Press **F12**, choose *Run a Game or Program*, and choose
   the ZIP: DOSBox Pure lists the programs in it; choose the game's.

## Keys

| Key or button                     | Does                                     |
|-----------------------------------|------------------------------------------|
| F12, gamepad Home or Select+Start | BMDOS's menu                             |
| F12, *DOSBox Pure's Menu and Keyboard* | DOSBox Pure's on-screen menu, keyboard and gamepad mapper |

Every other key is the PC's, F11 included.

See the [User Guide](docs/USER_GUIDE.md) for all the details, including the
options in `cmdline.txt`. All the documents are listed in
[docs/README.md](docs/README.md).

## Building

```
./build_sdcard.sh
```

builds the kernels for the Pi 4 and 5 (`./build_sdcard.sh pi5` builds one)
and stages the SD card files in `build/sdcard/`. It works on Linux and
macOS. See [docs/BUILDING.md](docs/BUILDING.md).

BMDOS can also run on a PC, in a simulator, for testing and debugging, and
live in a web page: see [docs/SIMULATOR.md](docs/SIMULATOR.md).

## How it fits together

```
src/main.cpp, kernel.cpp   Circle kernel: USB, GPIO, SD card; runs DOSBox Pure on core 1
src/network.cpp, webui/    Ethernet / Wi-Fi and the web UI, from BMC64
src/host.h                 What the kernel offers the emulator
src/port/platform.c        The libretro front end: runs the core; its picture,
                           sound, keyboard, mouse and gamepads
src/port/coreopts.c        DOSBox Pure's settings, kept in bmdos.cfg
src/port/vfs.c             The libretro file interface, over the SD card and USB sticks
src/port/keys.c            USB keys to libretro's
src/port/clock.c           The core's time and sleeps
src/port/circle/           Threads (cooperative, on libco) and POSIX functions
                           for the core on Circle
src/port/menu.c, osd.c     The menu
src/port/dosbox.mk         DOSBox Pure's source list and flags
third_party/dosbox-pure    DOSBox Pure (submodule, from Codeberg)
third_party/circle-stdlib  Circle and newlib (submodule)
src/av/, src/v3d/, src/kms Shared Pi 4/5 video, GPU and display drivers
tools/hostsim              Runs the emulator port on a PC for testing
```

DOSBox Pure has four small changes (`src/patches/`): a path on a USB
stick, `USB:/...`, is a full path, its 3dfx Voodoo draws triangles on the
Pi's other cores, a game started after another is new content, and its
automatic cycles no longer drop when a game waits;
everything else specific to BMDOS is in `src/`.

## Licence

BMDOS is free software under the GNU General Public License version 3;
see [LICENSE](LICENSE). It includes software from other projects under
their own licences (DOSBox Pure under the GPL version 2 or later, with
parts under the LGPL 2.1 or later, MIT, BSD and public domain; libco
under the ISC licence); see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
