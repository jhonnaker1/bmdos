# BMDOS User Guide

BMDOS turns a Raspberry Pi 4 or 5 into an MS-DOS PC for DOS games. It runs
the DOSBox Pure emulator on the bare hardware, so it starts in a couple of
seconds and can be switched off at any time.

BMDOS 0.1.0 has been tried on a Pi 5 and a Pi 4 (see the
[changelog](../CHANGELOG.md) for what was tested where); everything also
runs in the PC simulator (`tools/hostsim`, see [SIMULATOR.md](SIMULATOR.md)).

## The SD card

| File or folder   | What it is                                                  |
|------------------|-------------------------------------------------------------|
| `kernel8-rpi4.img`, `kernel_2712.img` | BMDOS for the Pi 4 and Pi 400, and the Pi 5, 500 and 500+ |
| `bootcode.bin`, `start.elf`, `fixup.dat`, `start4.elf`, `fixup4.dat`, `armstub8-rpi4.bin`, `*.dtb`, `overlays/` | Raspberry Pi firmware |
| `config.txt`     | Raspberry Pi firmware settings: the display mode            |
| `cmdline.txt`    | BMDOS's options (see Options)                               |
| `bmdos.cfg`      | Settings changed in BMDOS's menu, and DOSBox Pure's (written by BMDOS) |
| `dos/`           | For your games and programs (any folder works)              |
| `dos/system/`    | DOSBox Pure's own files: operating system images, MIDI sound fonts and ROMs (see its README) |
| `dos/saves/`     | DOSBox Pure's saves: what a game writes, and saved states   |
| `firmware/`      | Wi-Fi firmware                                              |

DOSBox Pure writes saves straight to the card, and BMDOS pushes everything
out to the card every second, so the Pi can be switched off at any time.

## Games and programs

BMDOS runs what DOSBox Pure can: a game's **ZIP** (as it is downloaded,
or a `.dosz`, a ZIP by another name), a **DOS program** (`.exe`, `.com`,
`.bat`: its folder becomes the PC's C: drive), a **CD image** (`.iso`,
`.cue`, `.chd`, `.ins`), a **floppy or hard disk image** (`.img`, `.ima`,
`.vhd`, `.jrc`, `.tc`), a list of disks (`.m3u`) or a DOSBox configuration
(`.conf`). No games come with BMDOS.

Choose one in the menu: **F12**, *Run a Game or Program*, and browse the
card (or a USB stick: *[ Drives ]* at the top of the card's list). The PC
starts again with it, and BMDOS starts with it from then on (it is kept
in `bmdos.cfg` as `content=`). Or click it in the web UI's file list.
*Run Nothing* starts the PC with nothing.

A ZIP's files are the PC's C: drive, read only: what the game writes goes
to `dos/saves` (a ZIP of its own per game), never into the game's ZIP.
DOSBox Pure's **start menu** lists the programs it found; choose one with
the arrows (or the gamepad) and Enter. Right (or the gamepad's right)
sets it to start by itself next time; *Go to Command Line* gives the DOS
prompt. When the program ends, the start menu comes back.

## The menu

Press **F12** (or a gamepad's Home button, or Select and Start together):

- **Run a Game or Program**, **Run Nothing**, **Restart the PC** (as
  switching it off and on, with the same game)
- **DOSBox Pure's Menu and Keyboard**: DOSBox Pure's own on-screen menu
  (its start menu, its on-screen keyboard and its gamepad mapper)
- **DOSBox Settings**: all of DOSBox Pure's settings, in its own
  categories, with its own descriptions: the emulated CPU's speed
  (*Emulated Performance*), the machine (SVGA, CGA, Hercules, Tandy), the
  sound cards, memory, the mouse and more. Most take effect at once; the
  rest when the PC starts again (*Restart the PC*). They are kept in
  `bmdos.cfg` as `dosbox_pure_...=` lines.
- **Gamepads**: what the first two gamepads are to the PC (below)
- **Video**, **Sound**, **Network**, **About**, **Reboot**, **Shut Down**

The arrow keys move, Enter chooses, Esc goes back, F12 closes. A gamepad
works too: the D-pad moves, A chooses, B goes back.

## Keyboard, mouse and gamepads

A USB keyboard is the PC's keyboard, every key but F12. A USB mouse is the
PC's mouse (DOSBox Settings, Input, for its speed and how it moves).

A USB gamepad (up to four) can be, in the menu's *Gamepads*:

- **Auto** (the default): DOSBox Pure's gamepad mapper. For the games it
  knows (many), the gamepad plays them as they would be played; for
  others, the D-pad is the arrow keys and the buttons Enter, Space and
  other keys. Change it in DOSBox Pure's Controller Mapper (in its start
  menu, on the right), which keeps the mapping with the game.
- **Keyboard Keys**: always the arrows and keys, never a game's mapping.
- **PC Joystick 1** or **2**: a PC joystick in the game port, its stick
  the D-pad (or left stick) and A and X its buttons: for games that ask
  for a joystick.
- **Gravis GamePad**, **Thrustmaster Flight Stick**, **Mouse**, **Off**.

A joystick wired to the GPIO pins (BMC64's wiring) joins the first or
second gamepad, with `gpio_joysticks=1` in `cmdline.txt`.

## Display

The PC's picture keeps its shape (4:3, as a PC monitor's) whatever its
resolution: 320x200, 640x400 text, 640x480, SVGA up to 1280x1024. The
display runs at 60 Hz; DOSBox Pure fits the PC's frames to it (VGA's are
70 Hz), as it does in RetroArch.

On a display that can show it, the picture is in 32-bit colour; with the
CRT effect (Video), in 16-bit.

## Sound

DOSBox Pure's sound comes out over HDMI (or the Pi 4's headphone jack),
at 48 kHz. Which sound cards the PC has is in DOSBox Settings, Audio
(Sound Blaster 16 by default, with AdLib music, and a Gravis Ultrasound if
you want one). For General MIDI music, put a sound font (`.sf2`) in
`dos/system` and choose it there as the MIDI output.

## Networking and the web UI

BMDOS can join your network over Ethernet or Wi-Fi and serve a web page
for managing the SD card from any browser on the same network, so files can
be copied to and from the Pi without taking the card out. It is the web UI
from BMC64. (The PC itself has no network card; its way onto the network
is FujiNet, below.)

Turn it on in the menu's *Network*. For Ethernet, set *Network* to Ethernet
and reboot. For Wi-Fi, choose *Scan for Wi-Fi Networks*: the first time,
it offers to switch Wi-Fi on and reboot; after that it lists the networks
it can hear, strongest first. Pick yours: it fills in *Wi-Fi Network*, and
if the network needs a password, *Wi-Fi Password* is next. Then reboot.
Set *Wi-Fi Country* to your two letter country code too; it decides which
channels are allowed. A hidden network can be typed in under *Wi-Fi
Network* instead. Or, in `cmdline.txt` (on the same single line as the
other options):

- Ethernet: `network=ethernet`
- Wi-Fi: `network=wifi`, and copy `wpa_supplicant.conf.example` to
  `wpa_supplicant.conf` in the root of the card with your network's name,
  password and country.

Wi-Fi needs the files in the `firmware` folder, which `build_sdcard.sh` puts
there.

Once the Pi has an address, it is shown at the bottom of the screen for a
few seconds (`Web UI: http://192.168.1.50/`) and written to the log.
Open that address in a browser.

The web UI has:

- **Dashboard**: the Pi's model, address, uptime, temperature, power and
  throttling warnings and free space on the card. Buttons to **Restart**
  the PC (as switching it off and on), **Reboot** BMDOS, **Shut Down**
  (the Pi halts; switch it off, then on again to restart; a Pi 400
  switches itself off, and its power key starts it again), and **Disable
  Web UI** until the next reboot.
- **Files**: browse the SD card and USB sticks (a button for each drive,
  while a stick is plugged in; reload the page after plugging one in);
  upload (or drag and drop) files into a folder, download, rename, delete,
  and create folders. Clicking a game's ZIP, a program or a disk image
  **starts it**, as *Run a Game or Program* does. `cmdline.txt`,
  `config.txt`, `bmdos.cfg` and `wpa_supplicant.conf` can be **edited** in
  the browser; changes take effect after a reboot, and the previous
  version is kept as `.bak`.

Uploads can't overwrite `cmdline.txt`, `config.txt` or `wpa_supplicant.conf`
(edit them instead), or anything in `firmware`. Everything else, including
the kernels, can be replaced, so a new build of BMDOS can be uploaded and
started with **Reboot**. A mistake in `cmdline.txt` or `config.txt` can stop
the Pi starting, and then needs the card in a computer (the `.bak` copy
makes that easy).

The web UI is open to anyone on your network. To protect it with a PIN, set
`webui_pin=1234` in `cmdline.txt`; the browser then asks for a user name
(anything) and the PIN. To network without the web UI, set `webui=0`.

The network also sets the Pi's clock, so files saved on the card get the
right date and time, and DOS's clock is right. Set `timezone` to your
offset from UTC in minutes, for example `timezone=600` for UTC+10.

## FujiNet

A [FujiNet](https://fujinet.online/) on a PC's serial port mounts disk
images from TNFS servers on the internet and on your network as DOS
drives, and gives DOS programs written for it a network device (HTTP,
TCP and more), a clock and a printer. MS-DOS reaches it with its driver
and tools, from [fujinet-msdos](https://github.com/FujiNetWIFI/fujinet-msdos):
`FUJINET.SYS` (the drives), `FMOUNT`, `NGET`, `NCOPY`, `FNSHARE` and more.

BMDOS works with **FujiNet-PC**, FujiNet's program for a computer, built
for the PC's serial port (RS-232) and running on a computer on the same
network. FujiNet-PC listens for the emulator's serial port over TCP
("BoIP"), and BMDOS connects to it: what the PC sends out of COM1 (or
COM2) goes to FujiNet-PC, and what FujiNet-PC sends comes in, as over a
serial cable. A FujiNet device for the PC (the RS-232 one) can't be used:
it has no way to reach an emulator.

1. On the computer, build FujiNet-PC for RS-232 from
   [fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware)
   (`./build.sh -p RS232`; see its `build-sh.md`: it needs CMake, a C++20
   compiler and MbedTLS 3, `brew install mbedtls@3` on a Mac). The program
   and its files end up in `build/dist`.
2. In `build/dist/fnconfig.ini`, under `[BOIP]`, turn it on and let it
   listen on the computer's network, not only for programs on the
   computer itself (`localhost`, the default); leave `port` empty for
   1985:
   ```
   [BOIP]
   enabled=1
   host=*
   port=
   ```
3. Start it (`./run-fujinet` in `build/dist`).
4. In BMDOS's menu, *Network*: turn *FujiNet (FujiNet-PC)* on, set
   *FujiNet-PC's Address* to the computer's address (`192.168.1.20`, or
   `192.168.1.20:1985` for another port), and *FujiNet's Port* to COM1
   (the default of FujiNet's DOS tools, `FUJI_PORT=1`) or COM2. Reboot.
   The *Network* page then says *Connected to 192.168.1.20, on COM1*.
   BMDOS tries again every 5 seconds while it can't connect.

In DOS, load `FUJINET.SYS` and use FujiNet's tools as on a real PC with a
FujiNet (the tools reach FujiNet through the driver's INT F5). DOSBox
Pure's own DOS doesn't read `CONFIG.SYS`, and has no room for a driver's
drives, so boot a real DOS from a hard disk image of your own in
`dos/system` (DOSBox Pure's start menu offers it: *Run Installed Operating
System*), with the driver on it and `DEVICE=C:\FUJINET.SYS` in its
`CONFIG.SYS`. The driver is on FujiNet-PC's own DOS disk, which it mounts
in its first slot at start, and from
[fujinet-msdos's releases](https://github.com/FujiNetWIFI/fujinet-msdos/releases).

Tried in the PC simulator with MS-DOS 6.22 and FujiNet-PC v1.6
(fujinet-firmware 21137d7): the driver found FujiNet, set DOS's clock
from it and attached drives E: to L:; `DIR E:` lists FujiNet's DOS disk
(CONFIG, FMOUNT, NGET, NCOPY, FNSHARE and the drivers), and `E:CONFIG`
shows FujiNet's hosts and drive slots. FreeDOS 1.4 works the same way:
its *LiteUSB* edition (`FD14LITE.img`, a 32 MB hard disk image, from
freedos.org) in `dos/system`, with `DEVICE=\FUJINET.SYS` added to its
`FDCONFIG.SYS` and `SETUP.BAT` renamed (else it starts the installer).
FreeDOS's own CD-ROM driver (UDVD2) then finds DOSBox Pure's CD drive as
well. A FreeDOS 1.3 hard disk image made by 86Box (4 GB, FAT32) didn't
start in DOSBox Pure ("read error while reading drive", before FreeDOS
itself).

Settings: `fujinet=1`, `fujinet_host` (the computer's address),
`fujinet_port` (1985 by default), `fujinet_com` (1 or 2).

## USB sticks

Up to three USB sticks (FAT32 or exFAT) can hold games
as well as the card: they are `USB:`, `USB2:` and `USB3:`, under *[ Drives ]*
in the menu's file browser and as buttons in the web UI. A stick can be
plugged in and pulled out while BMDOS runs; pulling out the one the game
is on starts the PC again without it.

## Options

`cmdline.txt` is a single line of `name=value` options separated by spaces.
BMDOS's menu saves the options it changes to `bmdos.cfg`, one
`name=value` per line, and those win over `cmdline.txt`. Either file can be
edited from the web UI. DOSBox Pure's own settings are in `bmdos.cfg` too
(the menu's DOSBox Settings).

| Option              | Values                       | Default  | What it does |
|---------------------|------------------------------|----------|--------------|
| `content`           | a path                       | none     | What the PC runs (the menu's Run a Game or Program) |
| `pad1_type`, `pad2_type` | `auto`, `keyboard`, `joystick1`, `joystick2`, `gravis`, `thrustmaster`, `mouse`, `off` | `auto` | What the first and second gamepads are to the PC (Gamepads) |
| `mouse_speed`       | percent                      | `100`    | The USB mouse's speed, before DOSBox Pure's own |
| `aspect`            | number                       | `1.0`    | Multiplies the width of the picture |
| `scale`             | 0.1 to 1.0                   | `1.0`    | Height of the picture as a fraction of the screen (Video, Screen Size; less for a TV that cuts off the edges) |
| `smooth`            | `0`, `1`                     | `0`      | Bilinear filtering instead of sharp pixels |
| `crt_shader`        | `0`, `1`                     | `0`      | CRT look (scanlines) from BMC64 |
| `crt_curvature`     | `0`, `1`                     | `0`      | With `crt_shader`, a curved screen |
| `crt_gpu`           | `0`, `1`                     | `1`      | Pi 4 and 5: the 3D GPU draws the smooth and CRT pictures; `0` has the ARM draw them (see [PI4.md](PI4.md)) |
| `resolution`        | `auto`, `1920x1080`, `1280x720` | `auto` | Pi 5 only, with `video_driver=kms`: the display's resolution; `auto` keeps the one the firmware chose (the Pi 5's firmware ignores `hdmi_mode`) |
| `video_driver`      | `kms`, `hvs`, `firmware`     | `kms`    | Pi 4 and 5: `kms` sets the display mode itself (60 Hz); `hvs` leaves the mode set in `config.txt`; `firmware` uses the firmware's framebuffer, if the others give trouble (see [PI4.md](PI4.md) and [PI5.md](PI5.md)) |
| `sound`             | `0`, `1`                     | `1`      | Sound on or off |
| `volume`            | 0 to 100                     | `100`    | Sound volume |
| `audio_out`         | `auto`, `hdmi`, `headphones` | `auto`   | Where the sound goes (`auto`: HDMI, or the headphone jack if the display takes no sound) |
| `audio_buffer_ms`   | 20 to 500 (at least 32 on the Pi 4, 40 on the Pi 5) | `80`     | Sound buffer; raise it if the sound breaks up |
| `core_debug`        | `0`, `1`                     | `0`      | DOSBox Pure's debug messages in the log too |
| `debug_cycles`      | `0`, `1`                     | `0`      | DOSBox Pure's automatic CPU speed in the log: what it measured and chose, every 4 frames |
| `debug_keys`        | `0`, `1`                     | `0`      | Every key pressed in the log, for a key that doesn't reach the PC (it logs what is typed) |
| `gpio_joysticks`    | `0`, `1`                     | `0`      | Joysticks wired to the GPIO pins (see Keyboard, mouse and gamepads) |
| `enable_logging`    | `off`, `uart`, `file`        | `off`    | Where the log goes (see Logging) |
| `network`           | `off`, `ethernet`, `wifi`    | `off`    | Networking (see Networking and the web UI) |
| `webui`             | `0`, `1`                     | `1`      | Web UI when networking is on |
| `webui_pin`         | digits                       | none     | PIN for the web UI |
| `timezone`          | minutes east of UTC          | `0`      | Time zone for the network clock |
| `usbpowerdelay`     | milliseconds                 | Circle's | Circle: wait for USB devices to power up (by default 300 ms, 510 ms behind a hub) |
| `task_time_us`      | microseconds                 | `1000`   | The least time the network, Wi-Fi and the web UI get each frame; `0` is one turn a frame |
| `fujinet`           | `0`, `1`                     | `0`      | Connect to FujiNet-PC, a FujiNet on the PC's serial port (see FujiNet) |
| `fujinet_host`      | address                      | none     | The computer FujiNet-PC runs on |
| `fujinet_port`      | port number                  | `1985`   | FujiNet-PC's BoIP port |
| `fujinet_com`       | `1`, `2`                     | `1`      | Which COM port FujiNet is on |

## Logging

BMDOS logs what it is doing, which helps when something doesn't work:
the version, the display and sound set up, what the PC runs, devices
coming and going, network status, and DOSBox Pure's own messages (which
program runs, anything it didn't like). Every 10 seconds it notes how
many frames were shown against the display's refresh, how much of the
time DOSBox took, the Pi's CPU clock and temperature, and how full the
sound buffer stayed. Choose where it goes with `enable_logging` in
`cmdline.txt`:

- **Off** (the default, also when `enable_logging` isn't set): no log.
- **UART**: the serial port, GPIO14 TX and GPIO15 RX at 115200 baud
  (all boards, the Pi 5 too). Needs a USB serial adapter.
- **File**: the serial port, and `bmdos.log` in the root of the SD
  card. The file starts with what was logged since boot, is written to the
  card every few seconds (and is complete after a crash, a hang or
  switching off), and starts again at boot once it is over 1 MB. Download
  it from the web UI's file manager to send with a bug report. Writing it
  costs a little time on each frame, so switch it off once you have what
  you need.

## Troubleshooting

**A game runs too fast or too slow**: DOSBox Settings, Performance,
*Emulated Performance* sets the PC's speed; *AUTO* guesses from the game's
date. A game from the late 1990s may need more than the Pi can give: the
speed line in the log (and About) says how much of the time DOSBox takes.

**"DOSBox Pure did not start"**: the log says why. F12 opens the menu to
choose something else.

**"Not found: ..."**: the game chosen isn't on the card (or its USB stick
isn't plugged in). The PC starts with nothing; choose it again.

**No picture**: uncomment `hdmi_safe=1` in `config.txt`. If that works, your
display doesn't accept 1080p at 60 Hz; try another `hdmi_mode`.

**No sound over HDMI**: try `hdmi_drive=2` in `config.txt`, or
`audio_out=hdmi` in `cmdline.txt`. No music in a game: check its sound
setup program chose the Sound Blaster (port 220, IRQ 7, DMA 1, as DOSBox
Pure's are by default).

**Sound breaks up**: the Pi can't keep up with that game at that speed:
lower *Emulated Performance*; for the sound alone, raise
`audio_buffer_ms` in `cmdline.txt`, e.g. to 200.

**The TV cuts off the edges of the picture or the menu**: many TVs enlarge
the picture and crop its edges (overscan). Make *Video*, *Screen Size*
smaller (90% usually does), or turn overscan off on the TV ("Just Scan",
"Screen Fit" or "1:1").

**Keyboard or mouse not working**: unplug it and plug it in again; devices
can come and go while the emulator runs. Some keyboards need a longer
`usbpowerdelay` (add `usbpowerdelay=1000` to `cmdline.txt`).
