#
# dosbox.mk - DOSBox Pure's sources and flags, for the kernel's Makefile
# and the PC simulator's (tools/hostsim/Makefile).
#
# DOSBox Pure (third_party/dosbox-pure, a submodule) is built as its
# libretro core, as its own Makefile builds it: every .cpp file in its top
# folder and src/ (three folders deep), with __LIBRETRO__, as a static
# library (STATIC_LINKING) that BMDOS's frontend (src/port/platform.c)
# calls. Nothing in it is changed.
#
# DBP is the path to third_party/dosbox-pure (set by the includer).
#

DBP_SRCS := $(sort $(wildcard $(DBP)/*.cpp $(DBP)/src/*.cpp $(DBP)/src/*/*.cpp \
                              $(DBP)/src/*/*/*.cpp))
# BMDOS's own files built with DOSBox Pure's headers and flags (src/port).
BMDOS_DBP_SRCS := fujinet.cpp

# Relative to $(DBP), for the object files' names.
DBP_SRCS_REL := $(patsubst $(DBP)/%,%,$(DBP_SRCS))

# Its version, for the About page ("1.0-preview8").
DOSBOX_PURE_VERSION := $(shell sed -n 's/.*DOSBOX_PURE_VERSION_STR "\(.*\)".*/\1/p' \
                                  $(DBP)/dosbox_pure_ver.h 2>/dev/null)

# As DOSBox Pure's Makefile has them (release build).
DBP_CXXFLAGS := -O2 -DNDEBUG -std=gnu++11 -fomit-frame-pointer -fexceptions \
                -Wno-address-of-packed-member -Wno-format -Wno-switch -Wno-psabi \
                -ffunction-sections -D__LIBRETRO__ -DSTATIC_LINKING \
                -D_FILE_OFFSET_BITS=64 -I$(DBP)/include -w

# Its time, and its emulation thread's sleeps, are BMDOS's (src/port/clock.c):
# the kernel's clock on the Pi, the simulator's virtual one on a PC.
DBP_RENAMES := -Dclock_gettime=bma_clock_gettime -Dnanosleep=bma_nanosleep \
               -Dusleep=bma_usleep
