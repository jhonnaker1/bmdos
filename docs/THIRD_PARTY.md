# Third-Party Software

BMDOS incorporates software and other materials from third-party projects. Third-party components remain subject to their respective copyright and license terms.

The original copyright, license, and attribution notices supplied with these components are retained with their source where applicable. The notices and licences for code BMDOS adapts from other projects are in [`sdcard/licenses/`](../sdcard/licenses/), which becomes the `licenses` folder of the SD card.

## DOSBox Pure

BMDOS incorporates [DOSBox Pure](https://codeberg.org/schelling/dosbox-pure) by Bernhard Schelling, a fork of [DOSBox](https://www.dosbox.com/) by the DOSBox Team, under the GNU General Public License version 2 or (at your option) any later version. It is the `third_party/dosbox-pure` submodule (1.0-preview8 and one commit, 9cccfa2), built as its libretro core with one change (`src/patches/dosbox_pure_volume_path_patch.diff`: paths that start with a volume name, such as `USB:/`, are full paths); BMDOS's menu draws its text with DOSBox's VGA font (`src/ints/int10_memory.cpp`).

Parts of DOSBox Pure are under other licences, all compatible with the GPL version 3 BMDOS is under: Nuked OPL3 (Nuke.YKT, with Tony Gies's changes) and mt32emu (the Munt project) under the GNU Lesser General Public License version 2.1 or later; MAME's SN76496 and SAA1099 sound chips under the BSD 3-clause licence; TinySoundFont (Bernhard Schelling, after Steve Folta's SFZero), SIMDe's MMX functions and libretro-common under the MIT licence; miniz (Rich Geldreich) in the public domain; Nuked SC-55 (nukeykt) under the GPL version 2 or later. Its notice and this list are in [`sdcard/licenses/dosbox-pure.txt`](../sdcard/licenses/dosbox-pure.txt), with the GNU GPL version 2 in [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt) and the LGPL version 2.1 in [`sdcard/licenses/LGPL-2.1.txt`](../sdcard/licenses/LGPL-2.1.txt).

No DOS, ROMs or games come with BMDOS: DOSBox Pure has its own DOS. The MT-32's and SC-55's ROMs, sound fonts and operating system images are the user's own.

## libco

BMDOS runs DOSBox Pure's emulation thread as a coroutine with libco (`src/port/circle/libco/`), by byuu and the higan team, under the ISC licence, from the copy in [Amiberry](https://github.com/BlitterStudio/amiberry)'s libretro folder (by way of BMAmiga). Its notice is in [`sdcard/licenses/libco.txt`](../sdcard/licenses/libco.txt).

## BMNeXT

BMDOS is based on [BMNeXT](https://github.com/jhonnaker1/bmnext), the bare metal NeXT emulator for the Raspberry Pi, under the GNU General Public License version 3 (a clone of it with its history, version 0.3.0). Its kernel, display, sound, network, web UI, menu and test harness code is used here.

## BMAmiga

BMDOS's libretro front end (`src/port/platform.c`: its video, sound and input), its cooperative threads (`src/port/circle/threads.c`), its POSIX functions for Circle (`src/port/circle/posix.c`), the USB key table (`src/port/keys.c`) and `circle_jit_exec_patch.diff` come from [BMAmiga](https://github.com/jhonnaker1/bmamiga), the bare metal Amiga emulator for the Raspberry Pi, under the GNU General Public License version 3.

## BMMacintosh

The picture's layout on the display (`src/av/layout.h`) and the CRT effect's scanlines on big displays come from [BMMacintosh](https://github.com/jhonnaker1/bmmacintosh), under the GNU General Public License version 3.

## BMAtariST

BMNeXT, and so BMDOS, is based on [BMAtariST](https://github.com/jhonnaker1/bmatarist), the bare metal Atari ST emulator for the Raspberry Pi, under the GNU General Public License version 3.

## BMApple2

BMAtariST, and so BMDOS, is based on [BMApple2](https://github.com/jhonnaker1/bmapple2), the bare metal Apple II emulator for the Raspberry Pi, under the GNU General Public License version 3.

## BMCoco

BMApple2, and so BMDOS, is based on [BMCoco](https://github.com/jhonnaker1/bmcoco), the bare metal Tandy Color Computer emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network, web UI, menu and test harness code is used here.

## BMA800XL

BMCoco, and so BMDOS, is a fork of [BMA800XL](https://github.com/aminch/bma800xl), the bare metal Atari 800XL emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network and web UI code is used here, including its Raspberry Pi 4 and Pi 5 display, sound and GPU drivers, its Pi 5 fan control and its USB sticks, merged up to BMA800XL 1.4.6 (without its Updater, USB paddles, Trak-ball and R: device).

## circle-stdlib

BMDOS uses the [circle-stdlib project](https://github.com/smuehlst/circle-stdlib), v21, which includes [Circle](https://github.com/rsta2/circle) by Rene Stange and the newlib C library.

See [`third_party/circle-stdlib/`](https://github.com/smuehlst/circle-stdlib/tree/v21) for the version used by BMDOS and its accompanying copyright and license information. newlib's notices are in [`sdcard/licenses/newlib.txt`](../sdcard/licenses/newlib.txt) and [`sdcard/licenses/libgloss.txt`](../sdcard/licenses/libgloss.txt).

## GCC Runtime

BMDOS links runtime code from the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 15.2.rel1 (GCC), under the GNU General Public License version 3 with the GCC Runtime Library Exception 3.1. The toolchain's GCC licence section is in [`sdcard/licenses/GCC-Runtime.txt`](../sdcard/licenses/GCC-Runtime.txt).

## BMC64

BMDOS incorporates source code from the [BMC64 project](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors.

See [BMC64](https://github.com/randyrossi/bmc64) for its source and its accompanying copyright and license information.

BMC64-derived files retain their original license and attribution notices. For files licensed under Apache-2.0, see [the included license](../sdcard/licenses/Apache-2.0.txt); modifications are identified in their headers.

## Raspberry Pi Firmware

BMDOS includes the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`, and for the Raspberry Pi 4 `start4.elf` and `fixup4.dat`) and the Raspberry Pi 4 and Pi 5 device trees from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware).

Their licences are next to them, in [`sdcard/firmware/`](../sdcard/firmware/) and on the SD card: `LICENCE.broadcom` for the firmware and `COPYING.linux` for the device trees. See the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware/tree/master/boot) for more.

## crt-pi

BMDOS uses the crt-pi CRT shader by davej, as included in BMC64, under the GNU General Public License version 2 or later: on the ARM, and compiled for the Raspberry Pi 4 and Pi 5's 3D GPU from [`tools/v3dshader/crt.frag`](../tools/v3dshader/crt.frag). See [`sdcard/licenses/crt-pi.txt`](../sdcard/licenses/crt-pi.txt) and [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt).

## Mesa

BMDOS uses the [Mesa 3D graphics library](https://mesa3d.org/)'s Broadcom V3D drivers in three ways. The adapted files, with their copyright notices, and Mesa's MIT licence are in [`sdcard/licenses/Mesa-MIT.txt`](../sdcard/licenses/Mesa-MIT.txt).

- **Adapted into the Raspberry Pi 4 and Pi 5 kernels** ([`src/v3d/`](../src/v3d/)): control list packet layouts and packing rules, the rendering job, texture and tile layouts, and the GPU's device facts. The V3D 4.2 (Pi 4) material is from Mesa 26.2.4, the V3D 7.1 (Pi 5) material from Mesa 25.2.8.
- **Adapted into the shader compiler tool** ([`tools/v3dshader/`](../tools/v3dshader/)): the shader preparation that Mesa's drivers do before compiling. The tool is built on a PC against Mesa 25.2.8 and is not part of the SD card.
- **As the shader compiler:** the compiled shaders in `src/v3d/` (`shader_*.h`, `shader71_*.h`) are output of Mesa 25.2.8's V3D compiler. Compiling with Mesa doesn't make them Mesa code: the crt-pi shaders are under crt-pi's licence (above), the others are BMDOS's.

## Linux

For the 3D GPU of the Raspberry Pi 4 and Pi 5, BMDOS adapts parts of the Linux kernel's V3D driver and BCM2835 power domain driver ([`src/v3d/`](../src/v3d/)), from the [Raspberry Pi kernel](https://github.com/raspberrypi/linux/tree/rpi-6.18.y) (`rpi-6.18.y`), under the GNU General Public License version 2 or later. See [`sdcard/licenses/Linux-V3D.txt`](../sdcard/licenses/Linux-V3D.txt) and [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt).

The Raspberry Pi 4 and Pi 5 display implementation ([`src/kms/pi4/`](../src/kms/pi4/), [`src/kms/pi5/`](../src/kms/pi5/) and the HDMI sound in [`src/av/`](../src/av/)) was independently written using hardware information documented in the Linux vc4 driver (`rpi-6.18.y`). No code from that driver was copied or incorporated.

## Circle ARM stub

For the Raspberry Pi 4, BMDOS includes `armstub8-rpi4.bin`, built from [Circle's `boot/armstub`](https://github.com/rsta2/circle/tree/master/boot/armstub) source, under Circle's [GNU General Public License version 3](https://github.com/rsta2/circle/blob/master/COPYING).

## Wi-Fi Firmware

BMDOS includes the Wi-Fi firmware for the Raspberry Pi's wireless chip from the [RPi-Distro firmware-nonfree repository](https://github.com/RPi-Distro/firmware-nonfree), downloaded when BMDOS is built.

See the `firmware/` folder on the SD card for the Broadcom, Cypress and Synaptics licenses that accompany it.

The Pi 5's files (`brcmfmac43455-sdio.raspberrypi,5-model-b.*`) are copies of the `brcmfmac43455-sdio.*` files under the name Circle looks for on the Pi 5, under the same Cypress license.

## Other Third-Party Components

Additional third-party components and supporting materials may be present in the BMDOS source tree. Where such material is accompanied by its own copyright, license, or attribution notices, those notices and terms continue to apply.
