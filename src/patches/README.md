# BMDOS Patches

## Circle

The following patches are applied by `make_all.sh`, in this order, while
preparing the Circle dependency. They carry changes that are not part of the
checked-out upstream revision. `make_all.sh --skip-patches` (and
`build_sdcard.sh --skip-patches`) leave Circle as it is.

Patches are applied to the submodule revisions pinned by circle-stdlib v21
(Circle 51.1.1, tag `Step51.1.1`), the same revisions BMC64 uses.

| Patch | Description |
| --- | --- |
| `circle_bolt_keyboard_patch.diff` | Adds Logitech Bolt keyboard receiver support (from BMC64). |
| `circle_jit_exec_patch.diff` | Leaves normal memory after the kernel's code executable (Circle sets `PXN` on all of it in `lib/translationtable64.cpp`), so that DOSBox's dynamic recompiler can run the code it writes into the heap; device and coherent (DMA) memory stay never-executable. From BMAmiga (Amiberry's JIT). |
| `circle_linux_bug_patch.diff` | Turns off `BUG_ON`. Circle's Linux emulation, which VCHIQ uses for the display and sound, asserts with `BUG_ON` that it only runs on core 0; BMDOS calls it from core 1, where the emulator runs (as BMC64 does with VICE). Without it the Pi halts with `mutex.cpp(9): assertion failed`. From BMC64's `circle_patch.diff`. |
| `circle_fatfs_exfat_patch.diff` | Turns on FatFs's exFAT (`FF_FS_EXFAT` in `addon/fatfs/ffconf.h`): USB sticks over 32 GB usually come formatted exFAT. It changes FatFs's structures, so circle-newlib is built with it too (`make_all.sh` builds both). From BMAmiga (without its SD2: change: BMDOS doesn't mount the card's second partition). |
| `circle_fatfs_fastseek_patch.diff` | Turns on FatFs's fast seek (`FF_USE_FASTSEEK` in `addon/fatfs/ffconf.h`), for `newlib_fastseek_patch.diff`. It changes FatFs's `FIL`, so circle-newlib must be built with it too (`make_all.sh` builds both). |
| `circle_sysconfig_patch.diff` | Enables multi-core support and the USB FIQ in `sysconfig.h`. From BMC64's `circle_patch.diff`. Also saves the floating point registers on interrupts on the Pi Zero, as Circle already does on the Pi 2 and later with GCC 12 or newer; on its one core, interrupts would otherwise clobber the emulator's (BMC64's Pi Zero build does the same). |
| `circle_tcp_receive_window_patch.diff` | Adds `CTCPConnection::SetReceiveWindow()`, which sets the TCP receive window, in segments, for connections opened after it (1 to 44; Circle's default is 10). BMA800XL uses 10 on Ethernet and 44 (64,240 bytes, the largest that fits without window scaling) on Wi-Fi, for faster web UI uploads. From BMC64. |
| `circle_usb_mouse_usage_page_patch.diff` | Circle's USB mouse driver took usages 1 (Pointer) and 2 (Mouse), and X, Y and Wheel, from any usage page, so a receiver with a vendor-specific collection using those numbers before the real mouse (the Geezer 2.4G set, `1ea7:0066`) was read wrongly and its mouse never moved. Only the Generic Desktop page's usages count now, a report must have the mouse's report ID, and the layout starts cleared (as upstream Circle's 0ca3147). From BMAmiga. |

## circle-newlib

Applied by `make_all.sh` after Circle's, to circle-newlib (Circle's C
library), reset first in the same way.

| Patch | Description |
| --- | --- |
| `newlib_fastseek_patch.diff` | Fast seek for files of 1 MB or more opened as they are (disk images): their cluster map is made when they are opened, so a seek back no longer follows the cluster chain from the file's start, a read from the SD card for every 128 clusters (hundreds a seek in a 2 GB image). A file in fast seek mode can't grow, so the map is let go before a write or seek past its end, and before truncating. |
| `newlib_seek64_patch.diff` | 64-bit file positions in `lseek()` (`CGlueIO::LSeek()` and `_lseek()` took and gave an `int`, so a disk image's data past 2 GB couldn't be reached though FAT32's files go to 4 GB). A position past what FatFs can hold is `EOVERFLOW`, not wrapped round. From BMAmiga. |

## DOSBox Pure

Applied by `apply_dosbox_pure_patches.sh`, which `src/port/dosbox.mk` runs
whenever the Pi kernels' Makefile or the PC simulator's is read (so
`make_all.sh`, CI and `tools/hostsim/run_tests.sh` all get them). Each
patch is applied only if it isn't there yet, so DOSBox Pure isn't reset;
one that neither is there nor applies stops the build (see
`docs/UPDATING.md`). `--skip-patches` doesn't skip these.

| Patch | Description |
| --- | --- |
| `dosbox_pure_volume_path_patch.diff` | `fpath_nocase()` (`dosbox_pure_libretro.cpp`) took a path that doesn't start with `/` as relative to the content's folder, so a disk image (ISO, CUE, IMG) started from a USB stick, `USB:/NAME`, was looked for as `USB:/USB:/NAME` and didn't mount ("Unable to open CD-ROM image file"). A path that starts with a name and `:/` is a full path now. ZIPs weren't affected (DOSBox Pure opens them without it). Test `usbimage`. |
| `dosbox_pure_unload_patch.diff` | `retro_unload_game()` (`dosbox_pure_libretro.cpp`) forgets the content: the next `retro_load_game()` is new content, as after `retro_deinit()` and `retro_init()`. DOSBox Pure took any load after its first for the same content started again (RetroArch unloads the whole core before other content; BMDOS keeps it), so after Run a Game, Run Nothing or a game from a USB stick the content's name (which names its save file, `dos/saves/NAME.pure.zip`), path and disk images were the first game's: one game's changed files went into another's save, and Run Nothing ran the last game again. Test `nothing`. |
| `dosbox_pure_core_pool_patch.diff` | Built with `-DBMDOS_CORE_POOL` (`src/port/dosbox.mk`). The 3dfx Voodoo's triangle workers (`voodoo.cpp`) are BMDOS's core pool (`host_pool_run()`, `src/corepool.h`) instead of DOSBox Pure's threads: BMDOS's threads take turns on core 1, so on the Pi its workers couldn't run beside it (and spinning on `done[]` they would wait forever). Each triangle over 350 pixels is split between core 1 and the idle cores 2 and 3. It also times the Voodoo's triangles, the mixer (`mixer.cpp`) and the SoundFont synthesizer (`midi_tsf.h`) for the log's "PC parts" line (`bmdos_prof_*`, `src/port/platform.c`). The PC simulator's pool is threads (`BMDOS_SIM_POOL`); Tomb Raider's 3D draws the same with none and with two. |

What else DOSBox Pure needs on Circle is in `src/port/circle`.
