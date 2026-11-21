# Changelog

## 0.1.0 (unreleased)

The first BMDOS: a bare metal MS-DOS PC for DOS games, with
[DOSBox Pure](https://codeberg.org/schelling/dosbox-pure) (1.0-preview8,
9cccfa2) in place of BMNeXT's Previous.

- DOSBox Pure built unmodified as its libretro core, driven directly by
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

From BMMacintosh 0.3.1:

- The picture keeps its shape on 5:4 and 16:10 displays
  (`src/av/layout.h`), and the CRT effect has its scanlines on displays
  over 1080 lines

Left out: the PC's network (DOSBox Pure's modem and IPX need a libretro
frontend's netplay), OpenGL Voodoo (the software one is there), the
32-bit kernels' dynamic recompiler.

Tested in the PC simulator only (`tools/hostsim`, 36 checks; and EGA
Trek from a ZIP): DOSBox Pure's start menu and DOS prompt, the keyboard,
the PC speaker, a gamepad as keys and as a PC joystick, the menu and
settings, USB sticks, the web UI's requests. The Pi 4 and Pi 5 kernels
build; **not yet tried on a Pi**.

## Earlier: BMNeXT

BMDOS's history before this is BMNeXT's (and before it BMAtariST's,
BMApple2's, BMCoco's and BMA800XL's); see the git log and BMNeXT's
changelog.
