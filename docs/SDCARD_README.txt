BMDOS - bare metal MS-DOS PC emulator for the Raspberry Pi 4 and 5
===================================================================

Copy everything in this folder to the root of a FAT32 SD card and start a
Raspberry Pi 5, 500, 4 or 400 from it, with a USB keyboard (and a mouse
or gamepad if your games want one).

The PC is DOSBox Pure, with its own DOS: nothing else is needed. No games
come with BMDOS: copy a game's ZIP (as it is downloaded), a DOS program or
a disk image into the dos folder (or onto a USB stick), press F12, Run a
Game or Program, and choose it. DOSBox Pure's start menu lists the
programs in a ZIP: choose the game's (Right makes it start by itself next
time).

Keys:
  F12 (or a gamepad's Home, or Select+Start)   BMDOS's menu
  Every other key, F11 too                     the PC's
In the menu: DOSBox Pure's Menu and Keyboard (its on-screen keyboard and
gamepad mapper), DOSBox Settings (the PC's speed, sound cards, video...),
Gamepads (keys, or a PC joystick).

Networking: choose Ethernet or Wi-Fi under Network in the menu (or add
network=ethernet or network=wifi to cmdline.txt), then open the address
shown on screen in a browser to manage the files on this card and start
games. (The PC itself has no network.)

No need to shut down: just switch it off.

Licences: BMDOS is under the GPL version 3 (LICENSE.txt); the licenses
folder has the notices for other projects' code it includes (DOSBox Pure
and the rest), and the firmware folder the Raspberry Pi and Wi-Fi
firmware's licences.

Options are in cmdline.txt; see docs/USER_GUIDE.md in BMDOS's source
for all of them: https://github.com/jhonnaker1/bmdos
