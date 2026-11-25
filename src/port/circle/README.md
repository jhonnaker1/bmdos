# DOSBox Pure on Circle

DOSBox Pure's libretro core expects a desktop system: threads, a C library
with all of POSIX. On the Pi, BMDOS runs it on Circle with circle-stdlib's
newlib and the Arm toolchain's libstdc++, which have the C library but no
threads. This folder is what DOSBox Pure needs beyond those, so that it
builds without changes for Circle (the Makefile's `DBP_PI_FLAGS`; its two
changes, in `src/patches/`, are a fix for USB sticks' paths and the
Voodoo's workers on the other cores). From BMAmiga, where
Amiberry's libretro core runs on the same.

## Threads (`threads.c`, `libco/`)

POSIX threads, cooperative, on libco: a thread runs until it waits (a
mutex someone else holds, a condition, a join, a sleep), then the next
that can go on runs, all on the emulator's core. DOSBox Pure starts one
thread, DOSBox's, with a 2 MB stack (`pthread_attr_setstacksize()`, which
is honoured), and hands frames to it and back with two semaphores, a
mutex and a condition each (`include/dbp_threads.h`): so DOSBox runs
inside `retro_run()`. Mutexes and conditions are found by their address,
so the same code runs in the PC simulator, which uses it too (its
Makefile renames DOSBox Pure's pthread calls to `bma_pthread_*`).

A thread that spins on an atomic, waiting for another, never lets it run:
the Voodoo's drawing threads would, so they are BMDOS's core pool instead,
on the Pi's other cores (`src/patches/dosbox_pure_core_pool_patch.diff`,
`src/corepool.h`).

## Memory and the rest (`posix.c`)

- `mprotect()` succeeds and does nothing: DOSBox's dynamic recompiler
  asks for its code cache (from `malloc()`) to be executable, and
  `circle_jit_exec_patch.diff` makes the whole heap so.
- `posix_memalign()`: Circle's `memalign()` asserts (stopping every core)
  for alignments over a cache line, 64 bytes, so bigger ones are aligned
  inside a bigger block, and `free()` (wrapped, in `src/port/bigalloc.c`)
  frees the block (`bma_free_aligned()`).
- `ftime()`, which DOSBox's BIOS sets its clock with; `gettimeofday()`
  (wrapped) gives 0 until the kernel's timer exists.
- `timegm()`, `pread()`, `pwrite()`, `access()`, `usleep()` and `sleep()`,
  and stand-ins that fail for processes, shared libraries, terminals and
  `ioctl()` (Amiberry's needs; DOSBox Pure links only what it uses).

## Headers (`include/`)

Missing ones (`sys/mman.h`, `dlfcn.h`, `sys/ioctl.h` and the rest), and
newlib's with what they lack (`pthread.h` declares its functions,
`sys/dirent.h` has `d_type`, which `readdir()`, wrapped in
`src/port/fswrap.c`, fills in as unknown). `bma_circle.h`, included before
each of DOSBox Pure's files: large file names, `CLOCK_MONOTONIC` and
`_POSIX_MONOTONIC_CLOCK` (for libretro-common's timer), endian functions,
and `std::mutex` and `std::condition_variable` over the threads above.
