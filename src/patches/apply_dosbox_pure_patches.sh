#!/bin/sh
#
# apply_dosbox_pure_patches.sh - applies src/patches/dosbox_pure_*.diff to
# DOSBox Pure (third_party/dosbox-pure), each one only if it isn't there
# yet, so it can run before every build: the Pi kernels' Makefile and the
# PC simulator's both run it when they are read (make_all.sh, CI and
# tools/hostsim/run_tests.sh all build through them).
#
# A patch that is neither applied nor applies cleanly (DOSBox Pure updated
# under it) stops the build: see docs/UPDATING.md.
#
# This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
# Raspberry Pi. See LICENSE for licensing details.
#

HERE=$(cd "$(dirname "$0")" && pwd)
DBP=$HERE/../../third_party/dosbox-pure

if [ ! -f "$DBP/dosbox_pure_libretro.cpp" ]; then
    echo "apply_dosbox_pure_patches.sh: no DOSBox Pure in third_party/dosbox-pure" >&2
    exit 1
fi
cd "$DBP" || exit 1
for p in "$HERE"/dosbox_pure_*.diff; do
    [ -f "$p" ] || continue
    if patch -p1 -R -s -f --dry-run < "$p" > /dev/null 2>&1; then
        continue
    fi
    if patch -p1 -s -f --dry-run < "$p" > /dev/null 2>&1; then
        patch -p1 -s -f < "$p" > /dev/null || exit 1
        echo "DOSBox Pure: applied $(basename "$p")" >&2
    else
        echo "apply_dosbox_pure_patches.sh: $(basename "$p") doesn't apply to DOSBox Pure" >&2
        exit 1
    fi
done
exit 0
