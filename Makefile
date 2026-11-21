#
# Makefile for the BMDOS kernel image.
#
# Run ./make_all.sh once first; it builds circle-stdlib and then this.
# After that "make" rebuilds the kernel image for the board circle-stdlib is
# built for: kernel7.img (Pi 2), kernel8-32.img (Pi 3), kernel8-rpi4.img
# (Pi 4, 64-bit) or kernel_2712.img (Pi 5, 64-bit).
# ./build_sdcard.sh stages everything for the SD card in build/sdcard.
#

ROOT := $(CURDIR)

ifeq ($(wildcard third_party/circle-stdlib/Config.mk),)
$(error circle-stdlib is not configured. Run ./make_all.sh first)
endif

include third_party/circle-stdlib/Config.mk
# RASPPI (the board) is in Circle's own Config.mk, which circle-stdlib's
# configure writes and Circle's Rules.mk reads; it's needed here first to
# pick the board's display and sound code.
include $(CIRCLEHOME)/Config.mk

BMDOS_VERSION ?= $(shell tools/version.sh --semver)
BMDOS_GIT ?= $(shell tools/version.sh --git)
DBP := third_party/dosbox-pure

include src/port/dosbox.mk

BUILD := build
DBP_OBJS := $(addprefix $(BUILD)/dbp/,$(DBP_SRCS_REL:.cpp=.o))
DBP_LIB := $(BUILD)/libdosboxpure.a
# BMDOS's Circle layer for the core: cooperative threads on libco, and the
# POSIX functions newlib lacks (src/port/circle/README.md).
CIRCLE_PORT_OBJS := $(BUILD)/port/threads.o $(BUILD)/port/posix.o $(BUILD)/port/libco.o
CIRCLE_PORT_LIB := $(BUILD)/libbmdoscircle.a

OBJS = src/main.o src/kernel.o src/logging.o src/options.o \
       src/port/platform.o src/port/coreopts.o src/port/vfs.o src/port/keys.o \
       src/port/clock.o src/port/menu.o src/port/osd.o src/port/bigalloc.o \
       src/port/fswrap.o \
       src/network.o src/network_time_sync.o \
       src/webui/webui.o src/webui/webui_http.o src/webui/webui_fs.o \
       src/webui/webui_machine.o src/webui/webui_assets.o \
       src/sdcard/sd_fs_fatfs.o src/sdcard/sd_fs_file.o

# Display and sound. The Pi 2 and 3 use the VideoCore through VCHIQ
# (dispmanx, from BMC64); the Pi 4 and 5 use Circle's framebuffer and HDMI
# sound drivers (src/av/, shared by both; see docs/PI4.md and docs/PI5.md).
ifneq ($(filter 4 5,$(RASPPI)),)
OBJS += src/av/video.o src/av/sound.o src/av/hdmisound.o src/av/scaler.o \
        src/kms/common/filter.o src/kms/common/modes.o \
        src/v3d/gpu.o src/v3d/draw.o src/v3d/crt.o \
        src/v3d/shaders.o
# The direct display driver: the Pi 4's (src/kms/pi4/) or the Pi 5's
# (src/kms/pi5/), with shared filter kernels and mode calculations.
ifeq ($(RASPPI),4)
OBJS += src/kms/pi4/dlist.o src/kms/pi4/hvs.o src/kms/pi4/display.o \
        src/kms/pi4/phy.o src/kms/pi4/modeset.o
else
OBJS += src/kms/pi5/dlist.o src/kms/pi5/hvs.o src/kms/pi5/display.o \
        src/kms/pi5/phy.o src/kms/pi5/modeset.o \
        src/pi45/board.o src/pi45/fan.o
endif
# Smooth scaling and the CRT shader are drawn by the ARM: -O3 lets GCC turn
# their per-pixel loops into NEON code (tools/pi4scaler tests the same).
src/av/scaler.o: OPTIMIZE = -O3
else
OBJS += src/fbl.o src/crt_pi_idx.o src/crt_pi_rgb.o src/vchiqsound.o \
        src/vchiqsoundbasedevice.o
endif

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(CIRCLEHOME)/addon/fatfs

# See src/port/fswrap.c, src/port/bigalloc.c, and posix_memalign() and
# gettimeofday() in src/port/circle/posix.c
LDFLAGS += --wrap=stat --wrap=readdir --wrap=malloc --wrap=free --wrap=gettimeofday

VERSION_DEFINES = -DBMDOS_VERSION=\"$(BMDOS_VERSION)\" \
                  -DBMDOS_GIT=\"$(BMDOS_GIT)\" -DDOSBOX_PURE_VERSION=\"$(DOSBOX_PURE_VERSION)\"
CFLAGS += $(VERSION_DEFINES)
CPPFLAGS += $(VERSION_DEFINES)

# The port's C files talk to DOSBox Pure through libretro.h.
src/port/%.o src/port/%.d: CFLAGS += -I src/port -I $(DBP)/libretro-common/include -std=gnu11

# DOSBox Pure, with Circle's compiler and C library (circle-stdlib's newlib
# and libstdc++): BMDOS's headers first (src/port/circle/include: pthread.h
# declares the functions src/port/circle/threads.c has, bma_circle.h what
# else newlib lacks), then Circle's (CPPFLAGS: circle-newlib's headers and
# libstdc++'s, not the toolchain's own newlib). Its dynamic recompiler is AArch64's on the 64-bit
# kernels (it writes code into the heap, which circle_jit_exec_patch.diff
# leaves executable); the 32-bit ones (the Pi 2 and 3) have none: the 32-bit
# one flushes the caches with a Linux system call.
DBP_PI_FLAGS = -I src/port/circle/include $(CPPFLAGS) \
               $(DBP_CXXFLAGS) $(DBP_RENAMES) -include src/port/circle/include/bma_circle.h
ifeq ($(strip $(AARCH)),32)
DBP_PI_FLAGS += -DDISABLE_DYNAREC=1
endif
CIRCLE_PORT_FLAGS = -I src/port/circle/include $(CFLAGS) \
                    -std=gnu11 -include src/port/circle/include/bma_circle.h \
                    -I src/port/circle/libco -Wall -Wno-unused-parameter

$(BUILD)/dbp_flags: FORCE
	@mkdir -p $(BUILD)
	@if [ "$$(cat $@ 2>/dev/null)" != "$(DBP_PI_FLAGS)" ]; then echo "$(DBP_PI_FLAGS)" > $@; fi

$(BUILD)/dbp/%.o: $(DBP)/%.cpp $(BUILD)/dbp_flags
	@mkdir -p $(dir $@)
	@echo "  CPP   $<"
	@$(CPP) $(DBP_PI_FLAGS) -c -o $@ $<

$(DBP_LIB): $(DBP_OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(DBP_OBJS)

$(BUILD)/port/%.o: src/port/circle/%.c $(wildcard src/port/circle/include/*.h)
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CIRCLE_PORT_FLAGS) -c -o $@ $<

$(BUILD)/port/libco.o: src/port/circle/libco/libco.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CIRCLE_PORT_FLAGS) -w -c -o $@ $<

$(CIRCLE_PORT_LIB): $(CIRCLE_PORT_OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(CIRCLE_PORT_OBJS)

.PHONY: FORCE
FORCE:

# The web UI's pages are compiled in. Regenerate them when they change.
WEBUI_ASSETS = $(shell find src/webui/assets -type f)
src/webui/webui_assets.c: $(WEBUI_ASSETS) tools/gen_webui_assets.py
	@echo "  GEN   $@"
	@python3 tools/gen_webui_assets.py

$(TARGET).img: $(DBP_LIB) $(CIRCLE_PORT_LIB)

ifneq ($(filter 4 5,$(RASPPI)),)
VIDEOCORE_LIBS :=
else
VIDEOCORE_LIBS := \
	$(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	$(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
	$(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
	$(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

LIBS := $(DBP_LIB) $(CIRCLE_PORT_LIB) \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libm.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libc.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libcirclenewlib.a \
	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/net/libnet.a \
	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
	$(CIRCLEHOME)/addon/wlan/libwlan.a \
	$(VIDEOCORE_LIBS) \
	$(CIRCLEHOME)/lib/sound/libsound.a \
	$(CIRCLEHOME)/lib/sched/libsched.a \
	$(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN += $(OBJS) src/*.o src/port/*.d src/*.d src/webui/*.d src/sdcard/*.d \
              src/av/*.o src/av/*.d src/kms/common/*.o src/kms/common/*.d \
              src/kms/pi4/*.o src/kms/pi4/*.d \
              src/kms/pi5/*.o src/kms/pi5/*.d src/v3d/*.o src/v3d/*.d \
              src/pi45/*.o src/pi45/*.d
.PHONY: distclean

distclean: clean
	rm -rf $(BUILD)

-include $(DEPS)
