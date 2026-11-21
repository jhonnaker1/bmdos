# Installing BMDOS

## What you need

- A Raspberry Pi 5, Pi 500 or Pi 500+, or a Raspberry Pi 4 Model B or Pi
  400.
- A microSD card (BMDOS itself needs only a few MB; DOS games are small,
  a CD game a few hundred MB).
- An HDMI display that accepts 1080p (or 720p) at 60 Hz.
- A USB keyboard; a USB mouse or gamepad if your games want one.
- DOS games: none come with BMDOS. A game's ZIP works as it is.

## Preparing the SD card

1. Format the card as FAT32.
2. Copy everything in the release zip (or in `build/sdcard/` after
   building) to the root of the card: the kernels (`kernel_2712.img`,
   `kernel8-rpi4.img`), the firmware, `config.txt`, `cmdline.txt` and the
   folders.
3. Copy your games to the card. The `dos` folder is there for them, but
   any folder works.

## First start

DOSBox Pure's start menu comes up, with nothing to run. Press **F12** for
BMDOS's menu: *Run a Game or Program* lists the card's folders; choose a
game's ZIP. The PC starts again with it, and DOSBox Pure's start menu
lists the programs in the ZIP: choose the game's (with the arrows and
Enter). BMDOS starts with that game from then on.

If you get no picture, see
[Troubleshooting](USER_GUIDE.md#troubleshooting) in the User Guide.

## Networking (optional)

To manage the card's files from a browser, and start games from it, turn
on Ethernet or Wi-Fi under *Network* in the F12 menu, then reboot; the
web UI's address is shown at the bottom of the screen. For Wi-Fi, *Scan
for Wi-Fi Networks* lists the networks in range and asks for the password.
See [Networking and the web UI](USER_GUIDE.md#networking-and-the-web-ui).

## Upgrading

Copy the new release's files over the old ones, except for files you have
changed: `config.txt` and `cmdline.txt` if you edited them. Your settings
(`bmdos.cfg`, `wpa_supplicant.conf`), your games and DOSBox Pure's saves
(`dos/saves`) are not in the release zip and stay as they are.

With networking on, a new kernel (`kernel_2712.img` for a Pi 5, 500 or
500+, `kernel8-rpi4.img` for a Pi 4 or 400) can also be uploaded in the
web UI's file manager and started with **Reboot**. Updates that change
other files (the Pi firmware in the root or in `firmware/`) need the card
in a computer.
