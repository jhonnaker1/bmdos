# What BMDOS inherits

BMDOS was started on 2026-11-20 as a clone of **BMNeXT 0.3.0** (with its
full history), the member of the family that had every recent fix from
the parents and siblings below. This is the survey made before choosing
it: what each parent and sibling had that BMDOS should have, and where
it is in BMDOS.

## Parents

| Project | Newest (checked 2026-11-20) | What BMDOS inherits | In BMDOS |
| --- | --- | --- | --- |
| BMA800XL (aminch/bma800xl) | 1.4.6 released 2026-11-15 (1.4.7 below) | 1.4.3–1.4.6: USB sticks and the file browser's [ Drives ] page, the Pi 5's fan (`src/pi45/`), `circle_tcp_receive_window_patch`, web UI uploads, idle connections and sorting. 1.4.7: idle connections kept open until a request arrives (Safari). bda317d (2026-11-20): the TSan "unsupported VMA range" fallback in `tools/pi4scaler`, which came from BMNeXT | All in, through BMNeXT 0.3.0 |
| BMA800XL | 1.4.7 released 2026-11-21 (checked 2026-11-25) | The sound buffer at least 40 ms on the Pi 5 and 32 ms on the Pi 4 (its Safari fix BMDOS has through BMNeXT) | `open_sound()` in `src/port/platform.c` |
| BMC64 (randyrossi/bmc64) | fc75101, 2026-11-19 (the same Safari fix, as aminch's PR #426) | The Safari fix (BMA800XL 1.4.7, above). Its Python release tests (`tools/automated_release_testing`) are BMC64's own | The fix, through BMNeXT |
| Circle (rsta2/circle) | Step 51.1.1, 2026-09-25 | Nothing new: the family is on it already, through circle-stdlib v21 (2026-09-25, its newest) | circle-stdlib v21 |

## Siblings

| Project | Newest | What BMDOS inherits | In BMDOS |
| --- | --- | --- | --- |
| BMNeXT | 0.3.0, 2026-11-19 | The clone's base: BMA800XL 1.4.3–1.4.6, USB sticks, the fan, `task_time_us` (from BMAmiga), the TSan fallback, CCPUThrottle at maximum on all boards, `bigalloc.c` (big blocks reused), the newlib seek64 and fast seek patches | Everything but Previous and its front end |
| BMMacintosh | 0.3.1, 2026-11-20 | `src/av/layout.h` (the picture keeps its shape on 5:4 and 16:10 displays, 301f4d5) and `CrtDrawSize()` (the CRT effect's scanlines on displays over 1080 lines, 3f84a50); `tools/qemu/run_qemu.sh` | Cherry-picked (BMDOS's second commit) |
| BMMacintosh | 0.2.0 | Only changed lines redrawn and core 3 presenting frames on the KMS path (4368ea8) | BMDOS's own: only changed lines copied (`PresentKms()`), three picture buffers; cores 2 and 3 draw the Voodoo's triangles instead (`src/corepool.h`) |
| BMAmiga | 0.6.2, 2026-11-18 | The libretro front end (`platform.c`: video, the dynamic rate control for sound, input, the environment callbacks), cooperative pthreads on libco (`src/port/circle/threads.c`), `posix.c`, `circle_jit_exec_patch` (code in the heap may run: DOSBox's dynamic recompiler), the HID to RETROK key table, the gettimeofday guard, `task_time_us` | Adapted in `src/port/` |
| BMAmiga | 0.6.2 | FatFs exFAT (`circle_fatfs_exfat_patch`, without its SD2: change) and the USB mouse usage page fix (`circle_usb_mouse_usage_page_patch`) | Taken 2026-11-25 |
| BMAmiga | 0.6.2 | newlib patches for open files (128), stat times, utime, ENOENT paths; FatFs UTF-8 names and the sector cache; `wait_for_network` | Not taken: Amiberry's folder drives needed them; DOSBox Pure reads ZIPs and images with 8.3 names. Candidates if a game needs them |
| BMAtariST | 0.1.1, 2026-11-10 | Nothing newer than BMNeXT has (settings save, states, big-block reuse came forward already) | — |
| BMTI994A | 0.1.0, 2026-11-10 | Nothing newer | — |
| BMLisa | 0.1.2, 2026-11-10 | The GCC missing-return lesson (`-Werror=return-type` worth keeping on for BMDOS's own code) | Kept in mind; DOSBox Pure builds with `-w` |
| BMApple2 | 0.4.4 (GSSquared work uncommitted) | Gamepad X/Y as buttons, Caps Lock light: Apple II specific | — |

## DOSBox Pure

[DOSBox Pure](https://codeberg.org/schelling/dosbox-pure) moved from
GitHub (schellingb/dosbox-pure, now a pointer) to Codeberg as
**schelling/dosbox-pure** in October 2026. BMDOS has it as a submodule at
9cccfa2 (1.0-preview8 and one commit, 2026-11-07). Its licence is the GNU
GPL version 2 **or later** (its README and every DOSBox Pure file's
notice), so it may be part of a GPLv3 kernel; the files in it under other
licences (LGPL 2.1 or later, MIT, BSD 3-clause, public domain) are listed
in `sdcard/licenses/dosbox-pure.txt`.

Its dynamic recompiler: **AArch64** (`risc_armv8le.h`) flushes the caches
with `dc civac`/`ic ivau`, which work bare metal, and allocates its code
cache with `malloc()`; with `circle_jit_exec_patch` the heap may hold code,
so the Pi 4 and 5 kernels have it. **AArch32** (`risc_armv4le-common.h`)
flushes them with a Linux system call (`swi`), so the 32-bit kernels (Pi 2
and 3) are built without it (`DISABLE_DYNAREC`).
