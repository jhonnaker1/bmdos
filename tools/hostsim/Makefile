#
# Builds the PC test harness for the emulator port (see host_sim.c).
#
#   make -C tools/hostsim
#   tools/hostsim/run_tests.sh
#
# DOSBox Pure is built as on the Pi (src/port/dosbox.mk), with the Pi's
# cooperative threads (src/port/circle/threads.c, tested here too: DOSBox
# Pure's pthread calls are renamed to them) and the simulator's virtual
# clock (src/port/clock.c). Its dynamic recompiler is left out on a Mac
# (Apple's Arm Macs won't run code written into ordinary memory); a Linux
# PC has its x86 one.
#

ROOT := $(abspath ../..)
DBP := $(ROOT)/third_party/dosbox-pure
include $(ROOT)/src/port/dosbox.mk

CC ?= cc
CXX ?= c++
OUT := build

BMA_THREADS := $(foreach f,create join detach self attr_init attr_destroy attr_setstacksize \
                 attr_getstacksize attr_setdetachstate mutex_init mutex_destroy mutex_lock \
                 mutex_trylock mutex_unlock cond_init cond_destroy cond_signal \
                 cond_broadcast cond_wait cond_timedwait,-Dpthread_$(f)=bma_pthread_$(f)) \
               -Dsched_yield=bma_sched_yield
ifeq ($(shell uname),Darwin)
DBP_SIM_FLAGS := -DDISABLE_DYNAREC=1 -Dclock_gettime_nsec_np=bma_clock_gettime_nsec_np
endif
DBP_ALL_FLAGS := $(DBP_CXXFLAGS) -g $(DBP_RENAMES) $(BMA_THREADS) $(DBP_SIM_FLAGS)

CFLAGS = -O2 -g -std=gnu11 -I $(ROOT)/src/port -I $(DBP)/libretro-common/include \
         -DBMDOS_HOST_LIBC -DDOSBOX_PURE_VERSION=\"$(DOSBOX_PURE_VERSION)\"

DBP_OBJS := $(addprefix $(OUT)/dbp/,$(DBP_SRCS_REL:.cpp=.o))
DBP_LIB := $(OUT)/libdosboxpure.a
PORT_OBJS := $(addprefix $(OUT)/port/,platform.o coreopts.o vfs.o keys.o osd.o menu.o clock.o) \
             $(OUT)/port/threads.o $(OUT)/port/libco.o
OBJS := $(PORT_OBJS) $(OUT)/host_sim.o $(OUT)/live.o

all: $(OUT)/bmdos_sim

$(OUT)/bmdos_sim: $(OBJS) $(DBP_LIB)
	$(CXX) -o $@ $(OBJS) $(DBP_LIB) -lpthread -lm

$(DBP_LIB): $(DBP_OBJS)
	@rm -f $@
	ar rcs $@ $(DBP_OBJS)

# The core's objects depend on its flags (a file holding them, rewritten
# only when they change).
$(OUT)/dbp_flags: FORCE
	@mkdir -p $(OUT)
	@if [ "$$(cat $@ 2>/dev/null)" != "$(DBP_ALL_FLAGS)" ]; then echo "$(DBP_ALL_FLAGS)" > $@; fi

$(OUT)/dbp/%.o: $(DBP)/%.cpp $(OUT)/dbp_flags
	@mkdir -p $(dir $@)
	@$(CXX) $(DBP_ALL_FLAGS) -c -o $@ $<

$(OUT)/port/%.o: $(ROOT)/src/port/%.c $(ROOT)/src/port/bmdos.h $(ROOT)/src/port/osd.h \
                 $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wextra -Wno-unused-parameter -c -o $@ $<

$(OUT)/port/threads.o: $(ROOT)/src/port/circle/threads.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(BMA_THREADS) -I $(ROOT)/src/port/circle/libco -Wall -Wextra -c -o $@ $<

$(OUT)/port/libco.o: $(ROOT)/src/port/circle/libco/libco.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g -c -o $@ $<

$(OUT)/host_sim.o: host_sim.c live.h $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/live.o: live.c live.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wextra -DLIVE_HTML=\"$(abspath live.html)\" -c -o $@ $<

clean:
	rm -r $(OUT)

FORCE:

.PHONY: all clean FORCE
