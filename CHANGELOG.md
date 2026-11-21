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
