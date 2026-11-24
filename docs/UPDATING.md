# Updating DOSBox Pure and Circle

## DOSBox Pure

DOSBox Pure is a submodule in `third_party/dosbox-pure`, from its home on
Codeberg (https://codeberg.org/schelling/dosbox-pure; the GitHub
repository it moved from, schellingb/dosbox-pure, is no longer updated).
It is built as its libretro core with one change of BMDOS's, a patch in
`src/patches` (`dosbox_pure_*.diff`, applied by
`src/patches/apply_dosbox_pure_patches.sh` whenever the Pi's or the
simulator's Makefile is read; see `src/patches/README.md`); everything
else BMDOS needs to run it on Circle is in `src/`.

| File | What it has to do with DOSBox Pure |
|---|---|
| `src/port/dosbox.mk` | Its source list (every `.cpp` in its top folder and `src/`, as its own Makefile has it), its flags, and the renames of its time and sleep calls |
| `src/port/platform.c` | The libretro frontend: the environment callbacks it asks (`core_environment()`), its picture (XRGB8888), sound, input, and running it a frame at a time |
| `src/port/coreopts.c` | Its settings, as it describes them (`RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`); BMDOS's own defaults for a few (`bmdos_option_bmdos_default()`) |
| `src/port/vfs.c` | The libretro VFS (version 3) it lists its system folder with |
| `src/port/circle/threads.c` | The pthreads its `include/dbp_threads.h` uses, cooperative, on libco: the emulation thread runs inside `retro_run()` |
| `src/port/circle/posix.c`, `include/` | What newlib lacks for it: `ftime()`, `mprotect()`, `posix_memalign()`, a `d_type` in `struct dirent` (with `readdir()` wrapped in `src/port/fswrap.c`) |
| `src/port/clock.c` | Its clock and sleeps (`clock_gettime`, `usleep`, `nanosleep`, renamed by `dosbox.mk`): the kernel's timer, or the PC simulator's virtual clock |
| `src/port/menu.c` | Its gamepad presets (`dosbox_pure_pad.h`'s `EPreset`, as libretro device subclasses in `platform.c`'s `pad_types`) |
| `src/port/osd.c` | Draws BMDOS's menu with DOSBox's VGA font (`int10_font_16`, `src/ints/int10_memory.cpp`) |

To move to a newer DOSBox Pure:

1. `git -C third_party/dosbox-pure fetch` and check out the release (or
   commit) wanted.
2. Read its changes for new libretro calls: `RETRO_ENVIRONMENT_*` it asks
   that `core_environment()` answers no to, files added under `src/` that
   need something newlib lacks, and changes to `dbp_threads.h` (BMDOS's
   threads are cooperative: a thread that spins on an atomic, waiting for
   another, would never let it run; the Voodoo's drawing threads are kept
   off for that, `dosbox_pure_voodoo_perf`).
3. Check the licences of any new files: BMDOS is GPLv3, so every file must
   be GPL 2 or later, or a licence compatible with it (list them in
   `sdcard/licenses/dosbox-pure.txt`).
4. Run `tools/hostsim/run_tests.sh` (and with `BMDOS_GAME=` a game or two),
   build both kernels (`./build_sdcard.sh`), and try a game on a Pi.
5. Commit the submodule:

   ```
   git add third_party/dosbox-pure
   git commit -m "Update DOSBox Pure to <version>"
   ```

## circle-stdlib (Circle and newlib)

circle-stdlib is a submodule in `third_party/circle-stdlib`, pinned to the
same revision BMC64 uses. `make_all.sh` applies the patches in `src/patches/`
to Circle and circle-newlib before building (see its README):

- `circle_sysconfig_patch.diff` enables multi-core and the USB FIQ
- `circle_linux_bug_patch.diff` turns off `BUG_ON`, whose checks in Circle's
  Linux emulation (used by VCHIQ for display and sound) only allow core 0;
  the emulator calls them from core 1, as BMC64 does
- `circle_bolt_keyboard_patch.diff` adds Logitech Bolt keyboard support
  (from BMC64)
- `circle_jit_exec_patch.diff` leaves the heap executable, for DOSBox's
  dynamic recompiler (from BMAmiga)
- the FatFs, newlib and TCP patches the rest of the family has

To update, move the submodule to a new circle-stdlib release, run
`git submodule update --init --recursive`, then `./build_sdcard.sh`. If a
patch no longer applies, update it from the matching BMC64 patch or by
hand. `src/fbl.cpp` and `src/vchiqsoundbasedevice.cpp` use Circle's VC4
addon and may need the same changes BMC64 makes when it updates Circle.
