# Changelog

## 0.1.1 – 2026-11-26

- DOSBox Pure's automatic cycles hold steady (`src/patches/dosbox_pure_auto_cycles_patch.diff`):
  they were cut while the emulation had time to spare, when a game spun
  on a port (Duke Nukem 3D waiting for its timer), and cut to the minimum
  after one long frame (a load); Duke's cycles swung between 10,000 and
  214,000 within seconds on the Pi 5. `debug_cycles=1` logs each decision
- Requests to the firmware from two cores at once no longer mix up
  (`circle_property_tags_lock_patch`): on the Pi 5 the fan's temperature
  reading on core 0 and the speed report on core 1 could swap answers
  (once the fan read 2,400,000 °C and went to full speed)
- A sweep: leftovers from the parent projects in comments, the user
  guide, the simulator's settings, compiler warnings

Tested on a Raspberry Pi 5 (8 GB): Duke Nukem 3D (63-88%, from 56-85%)
and the Tomb Raider demo (94-100%) with the automatic cycles; no wrong
clock or temperature readings and the fan normal with the firmware
requests locked. Also on the Pi 5 since 0.1.0's notes: Tomb Raider no
longer runs faster than 100% (the pacing fix 0.1.0 had untried), a
SanDisk USB stick mounted as `USB:`, the mouse. Not yet on a Pi 4: the
changes since 0.1.0. Not tried on a Pi: exFAT sticks, gamepads, FujiNet,
the web UI. Everything passes in the PC simulator (51 checks).

## 0.1.0 – 2026-11-25

The first BMDOS: a bare metal MS-DOS PC for DOS games, with
[DOSBox Pure](https://codeberg.org/schelling/dosbox-pure) (1.0-preview8,
9cccfa2) in place of BMNeXT's Previous.

- DOSBox Pure built as its libretro core, driven directly by
  BMDOS's front end (`src/port/platform.c`, after BMAmiga's): its picture
  (32-bit where the display shows it), its sound at the HDMI's 48 kHz
  with dynamic rate control, the keyboard, a mouse, and up to four
  gamepads. No SDL, no frontend library
- Its emulation thread on cooperative threads (libco, from BMAmiga), on
  the emulator's core; its files through the C library (the SD card and
  USB sticks), with the libretro VFS for its system folder
- The 64-bit kernels (Pi 4 and 5) have DOSBox's dynamic recompiler for
  AArch64 (`circle_jit_exec_patch`, from BMAmiga, lets it run code in the
  heap)
- One frame a display refresh: DOSBox Pure is told so (the libretro
  throttle state), and fits VGA's 70 Hz to the display's 60 Hz itself
- The menu (F12): Run a Game or Program (a ZIP, a program, a disk image,
  from the card or a USB stick), Run Nothing, Restart the PC, DOSBox
  Pure's own on-screen menu and keyboard, every one of DOSBox Pure's
  settings in its own categories, Gamepads (DOSBox Pure's mapper, keys,
  PC joysticks, Gravis, Thrustmaster, mouse), video, sound and network.
  A DOS look: the VGA's font, light grey on blue
- What the PC runs, and DOSBox Pure's settings, kept in `bmdos.cfg` (its
  settings were raised to 128, 48-character names and 520-character values
  for that); `SD:/dos/system` and `SD:/dos/saves` for DOSBox Pure's files
- The web UI starts games, programs and disk images; its theme is DOS's
- A USB stick pulled out with the game on it starts the PC again without
  it

- FujiNet on COM1 or COM2 (menu Network, `fujinet=1`, `fujinet_host`,
  `fujinet_port`, `fujinet_com`): BMDOS connects to FujiNet-PC built for
  RS-232 (`build.sh -p RS232`, `[BOIP] enabled=1`, port 1985) and carries
  the serial line's bytes both ways (`src/port/fujinet.cpp`, a DOSBox
  serial port of BMDOS's own). For fujinet-msdos's FUJINET.SYS and tools;
  the User Guide's FujiNet section says how.
  Tried in the simulator with a real FujiNet-PC: MS-DOS 6.22 with
  FUJINET.SYS (v1.6.1) attached FujiNet's drives E: to L:, read its DOS
  disk and ran its CONFIG program; FreeDOS 1.4 (LiteUSB) the same
- From BMCoco and BMApple2 (0.5.13, 0.4.6): changing a Wi-Fi setting in
  the menu no longer spoils a Wi-Fi key of 64 hex digits (as
  `wpa_passphrase` writes it) or a password with a `"` in it

- IRQ 13, 14 and 15 (INT 75h to 77h) get default handlers, as a real
  BIOS has: DOSBox Pure's BIOS leaves them at 0000:0000 while its IDE
  emulation raises IRQ 14 and 15, so FreeDOS's CD-ROM drivers UDVD2 and
  UIDE sent the PC to address 0 at the CD drive's first interrupt
  (FreeDOS 1.4 stopped while it started). `src/port/biosfix.cpp`; test
  `irq`. Still not working: ATAPICDD, which soft-resets the controller and
  waits for the reset signature that DOSBox Pure's (and DOSBox-X's) IDE
  emulation doesn't load after a soft reset

- A disk image (ISO, CUE, IMG) on a USB stick mounts: DOSBox Pure looked
  for `USB:/NAME` in the content's own folder (`USB:/USB:/NAME`) and
  said "Unable to open CD-ROM image file". A change to DOSBox Pure,
  `src/patches/dosbox_pure_volume_path_patch.diff`, applied by the
  Makefiles; test `usbimage`. ZIPs on sticks worked already
- The 3dfx Voodoo's triangles are drawn on three cores: core 1 and the
  Pi's otherwise idle cores 2 and 3 (`src/corepool.h`; DOSBox Pure's own
  worker threads can't run on BMDOS's cooperative threads,
  `src/patches/dosbox_pure_core_pool_patch.diff`). Not with smoothed or
  CRT pictures on the Pi 4, whose cores 2 and 3 draw those
- DOSBox Pure's automatic cycles fill 80% of a frame (Limit CPU Usage),
  not 100%: BMDOS's own work on the same core made frames late, and the
  sound crackled
- A frame that takes longer than one display refresh is shown at once
  instead of at the refresh after next: 3D games whose frames took a
  little over one ran at 55-75%
- A game started from the menu after another is that game: DOSBox Pure
  took it for the first one started again, so its changed files went
  into the first game's save (`dos/saves/`), and Run Nothing ran the last
  game again (`src/patches/dosbox_pure_unload_patch.diff`, test `nothing`)
- Three picture buffers on the Pi 4 and 5's direct display (two only if
  three don't fit in the Pi 5's framebuffer): with two, a frame waited for
  the vertical blank that showed the one before, and Tomb Raider spent a
  quarter of its time waiting (80-90% speed). A late frame is shown
  without waiting for the blank it missed, and three display list slots
  keep a second picture before a blank from overwriting the list shown
- From BMA800XL 1.4.7: the sound buffer is at least 32 ms on the Pi 4 and
  40 ms on the Pi 5 (`audio_buffer_ms`), as less breaks up
- From BMAmiga: USB sticks formatted exFAT (`circle_fatfs_exfat_patch`),
  and USB mouse receivers whose report descriptor has a vendor collection
  before the mouse (`circle_usb_mouse_usage_page_patch`)
- The Pi 4 runs at 1.8 GHz (`arm_freq=1800` in `config.txt`), the clock
  of the Pi 4 B from revision 1.4 on and of the Pi 400
- Every 10 s the log has "PC parts": how much of the time the Voodoo, the
  sound and the SoundFont synthesizer took

From BMMacintosh 0.3.1:

- The picture keeps its shape on 5:4 and 16:10 displays
  (`src/av/layout.h`), and the CRT effect has its scanlines on displays
  over 1080 lines

Left out: the PC's network (DOSBox Pure's modem and IPX need a libretro
frontend's netplay), OpenGL Voodoo (the software one is there), the
32-bit kernels' dynamic recompiler.

Tested on a Raspberry Pi 5 (8 GB) and a Pi 4 B (4 GB, at 1.8 GHz), HDMI
at 1920x1080, a USB keyboard and mouse, Wi-Fi, games from ZIPs on the SD
card:

- Commander Keen 1, Second Reality (Gravis UltraSound) and TopBench at
  full speed on both; Doom; One Must Fall 2097 (about 90%); Duke Nukem
  3D (about 85% on both); the Tomb Raider demo with the 3dfx Voodoo
  (full speed on the Pi 5, about 80-85% on the Pi 4). Sound without
  crackling, no tearing or flicker
- MS-DOS 6.22 and FreeDOS 1.4 started from their disk images
  (`dos/system`); a CD image (ISO) and a CUE/BIN with a CD audio track,
  heard, on the Pi 4
- Changing games from the menu, Run Nothing, Restart the PC; logging to
  the card; the time from the network

Not tried on a Pi: the last pacing fix (frames skipped for an unchanged
picture; Tomb Raider briefly ran at 101-106% before it), exFAT and FAT32
USB sticks, the USB mouse receiver fix, the sound buffer's floor,
gamepads, FujiNet, the web UI, the Pi 400, 500 and 500+. These, and
everything above, pass in the PC simulator (`tools/hostsim`, 51 checks).

## Earlier: BMNeXT

BMDOS's history before this is BMNeXT's (and before it BMAtariST's,
BMApple2's, BMCoco's and BMA800XL's); see the git log and BMNeXT's
changelog.
