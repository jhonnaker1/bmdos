/*
 * platform.c - the libretro host that runs DOSBox Pure for BMDOS.
 *
 * DOSBox Pure is a libretro core (third_party/dosbox-pure), built into the
 * kernel as a static library. This file is the "frontend" the core talks
 * to, on top of the Circle kernel services in src/host.h: it answers the
 * core's questions (where its files are, its settings: coreopts.c), takes
 * its pictures and sound, gives it the keyboard, mouse and gamepads, and
 * runs it a frame at a time (retro_run()). The menu is in menu.c and
 * osd.c; files in vfs.c.
 *
 * Threads: DOSBox Pure runs DOSBox on a thread of its own, which
 * retro_run() starts on a frame and waits for. Circle has none: they are
 * cooperative here (circle/threads.c, on libco), so DOSBox runs within
 * retro_run(), on the emulator's core.
 *
 * Timing: frames are shown one per display refresh (60 Hz), and BMDOS
 * tells the core so (RETRO_ENVIRONMENT_GET_THROTTLE_STATE): the core then
 * emulates a refresh's worth of the PC's time each frame and shows the
 * PC's frames (70 Hz for VGA's text and 320x200 modes) as they fall, as it
 * does in RetroArch with vsync. The sound is kept in step by dynamic rate
 * control (from BMAmiga).
 *
 * Content: what the PC runs is a ZIP of a game (or a .dosz), a DOS program,
 * a disk image (floppy .img/.ima, hard disk .img/.vhd, CD .iso/.cue/.chd)
 * or nothing, DOSBox Pure's start menu or its prompt. It is chosen in the
 * menu (or the web UI) and kept in bmdos.cfg (content=). DOSBox Pure's own
 * files are in SD:/dos/system (its "system directory": a FreeDOS or MS-DOS
 * boot disk image, Windows 9x hard disk images, MIDI sound fonts) and
 * SD:/dos/saves (saved games written to C:, saved states).
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libretro.h"

#include "../host.h"
#include "bmdos.h"
#include "osd.h"

#ifndef BMDOS_VERSION
#define BMDOS_VERSION "dev"
#endif

void bma_threads_yield(void);   /* circle/threads.c */
double bmdos_clock_now(void);   /* clock.c */
extern unsigned bmdos_sound_rate;   /* coreopts.c */

/* The frame buffer: big enough for SVGA's 1280x1024. */
#define FB_W 1280
#define FB_H 1024
/* The menu's area. */
#define OSD_W 640
#define OSD_H 460

/* ---- Settings ---------------------------------------------------------------- */

const char *bmdos_sd_root(void)
{
	return host_option("root", "/");
}

void bmdos_card_path(char *path, unsigned size, const char *dir, const char *name)
{
	const char *root = bmdos_sd_root();
	size_t len = strlen(root);
	snprintf(path, size, "%s%s%s%s%s", root, len > 0 && root[len - 1] == '/' ? "" : "/",
	         dir, dir[0] && name[0] ? "/" : "", name);
}

static int option_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "on") == 0
	    || strcmp(value, "yes") == 0 || strcmp(value, "true") == 0;
}

static void save_setting(const char *name, const char *value)
{
	host_option_set(name, value);
	host_options_save();
}

/* ---- Video ------------------------------------------------------------------- */

static uint16_t *fb_pixels;
static int fb_pitch_px;
static uint32_t *fb32_pixels;     /* the 32-bit frame buffer, NULL if none */
static int fb32_pitch_px;
static int depth = 16;            /* the one shown: 16 or 32 */
static int osd_view;
static int pic_w = 640, pic_h = 400;
static double pic_aspect = 4.0 / 3.0;
static double refresh_rate = 60.0;

uint16_t *bmdos_framebuffer(int *pitch_px)
{
	*pitch_px = fb_pitch_px;
	return fb_pixels;
}

static void apply_view(void)
{
	if (osd_view)
		host_video_set_view(0, 0, OSD_W, OSD_H, (double) OSD_W / OSD_H);
	else
		host_video_set_view(0, 0, pic_w, pic_h, pic_aspect * host_option_double("aspect", 1.0));
}

/* The 32-bit frame buffer where there is one and the CRT effect (which
   reads RGB565) is off. */
static void choose_depth(void)
{
	depth = fb32_pixels != NULL ? host_video_depth(32) : 16;
}

void bmdos_osd_view(int on)
{
	osd_view = on;
	/* The menu is drawn in RGB565. */
	if (on) {
		host_video_depth(16);
		depth = 16;
	} else
		choose_depth();
	apply_view();
}

static void clear_framebuffer(void)
{
	int y;
	for (y = 0; y < FB_H; y++) {
		memset(fb_pixels + y * fb_pitch_px, 0, FB_W * 2);
		if (fb32_pixels != NULL)
			memset(fb32_pixels + y * fb32_pitch_px, 0, FB_W * 4);
	}
}

/* Times a few vertical blanks to find the display's refresh rate. */
static void measure_refresh_rate(void)
{
	enum { SKIP = 3, COUNT = 20 };
	double start = 0.0, rate;
	int i;
	for (i = 0; i < SKIP + COUNT; i++) {
		host_video_present(TRUE);
		if (i == SKIP - 1)
			start = host_time();
	}
	rate = COUNT / (host_time() - start);
	if (rate > 20.0 && rate < 250.0)
		refresh_rate = rate;
	LOG("display refresh rate %.3f Hz\n", refresh_rate);
}

double bmdos_refresh_rate(void)
{
	return refresh_rate;
}

static void open_sound(void);

void bmdos_video_settings_changed(void)
{
	if (fb_pixels == NULL)
		return;
	clear_framebuffer();
	if (!osd_view)
		choose_depth();
	apply_view();
}

void bmdos_display_mode_changed(void)
{
	/* Where the host can set the video mode (the Pi 4 and 5), the display
	   runs at 60 Hz: VGA's 70 Hz is unusual for a TV. */
	if (host_display_match_rate(60.0))
		measure_refresh_rate();
	open_sound();
	apply_view();
}

/* A picture from the core (XRGB8888): into the frame buffer shown. */
static void core_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
	unsigned x, y;
	if (data == NULL || osd_view)
		return;   /* the same picture again, or the menu is showing */
	if (w > FB_W)
		w = FB_W;
	if (h > FB_H)
		h = FB_H;
	if (depth == 32) {
		for (y = 0; y < h; y++)
			memcpy(fb32_pixels + y * fb32_pitch_px, (const uint8_t *) data + y * pitch, w * 4);
	} else {
		for (y = 0; y < h; y++) {
			uint16_t *out = fb_pixels + y * fb_pitch_px;
			const uint32_t *in = (const uint32_t *) ((const uint8_t *) data + y * pitch);
			for (x = 0; x < w; x++) {
				uint32_t p = in[x];
				out[x] = (uint16_t) (((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x001f));
			}
		}
	}
	if ((int) w != pic_w || (int) h != pic_h) {
		pic_w = (int) w;
		pic_h = (int) h;
		apply_view();
	}
}

static void take_geometry(const struct retro_game_geometry *g)
{
	if (g->aspect_ratio > 0.1f && fabs(g->aspect_ratio - pic_aspect) > pic_aspect * 1e-4) {
		pic_aspect = g->aspect_ratio;
		apply_view();
	}
}

/* ---- Sound -------------------------------------------------------------------- */

static unsigned ao_rate = 48000;   /* the core's */
static unsigned ao_open;           /* the host's rate, 0 if there's no sound */
static unsigned ao_latency;        /* frames */
static int volume = -1;
static unsigned audio_this_frame;  /* frames the core gave this frame */
static const int16_t silence[2048];

int bmdos_volume(void)
{
	if (volume < 0) {
		volume = host_option_int("volume", 100);
		if (volume < 0 || volume > 100)
			volume = 100;
	}
	return volume;
}

void bmdos_set_volume(int v)
{
	char text[8];
	volume = v < 0 ? 0 : v > 100 ? 100 : v;
	snprintf(text, sizeof text, "%d", volume);
	save_setting("volume", text);
}

static void open_sound(void)
{
	unsigned ms = (unsigned) host_option_int("audio_buffer_ms", 80);
	if (ms < 20)
		ms = 20;
	if (ms > 500)
		ms = 500;
	ao_latency = ao_rate * ms / 1000;
	ao_open = host_sound_open(ao_rate, 2, ao_latency);
	if (ao_open == 0)
		LOG("no sound\n");
	else
		bmdos_sound_rate = ao_open;
}

/* Keeps the sound output fed while the PC isn't running. */
static void feed_silence(void)
{
	unsigned frames;
	if (ao_open == 0)
		return;
	frames = host_sound_free_frames();
	if (frames > ao_latency / 2) {
		frames -= ao_latency / 2;
		while (frames > 0) {
			unsigned n = frames > 1024 ? 1024 : frames;
			host_sound_write(silence, n);
			frames -= n;
		}
	}
}

/* Sound from the core, stereo. The PC runs at the display's pace, never
   quite the sound output's, so the buffer drifts. It is kept about half
   full by dynamic rate control (as RetroArch does; from BMAmiga): the
   sound is resampled all the time, by linear interpolation carried from
   batch to batch, at a rate a little over or under the core's, by up to
   0.5% (inaudible) as the buffer is under or over half full; 2% when it is
   nearly dry or full. */
#define AO_MAX 4096
#define AO_OUT_MAX (AO_MAX * 2 + 16)

static double ao_phase;           /* next output's position, in input frames from the batch's start */
static int16_t ao_last[2];        /* the previous batch's last frame (position -1) */
static double ao_ratio_sum;
static unsigned ao_ratio_count;
static unsigned ao_fill_min = ~0u, ao_fill_max;
static unsigned long long ao_frames_total;   /* for the simulator's tests */
static unsigned long long ao_loud_total;     /* frames not silent */

static size_t core_audio_batch(const int16_t *data, size_t frames)
{
	static int16_t buf[AO_OUT_MAX * 2];
	unsigned free_frames, used, n = (unsigned) frames, out = 0, c, i;
	int v = bmdos_volume();
	double error, ratio, step, p;
	if (ao_open == 0 || frames == 0)
		return frames;
	audio_this_frame += (unsigned) frames;
	ao_frames_total += frames;
	if (n > AO_MAX)
		n = AO_MAX;
	for (i = 0; i < n * 2; i++)
		if (data[i] > 64 || data[i] < -64) {
			ao_loud_total += n;
			break;
		}
	free_frames = host_sound_free_frames();
	used = free_frames < ao_latency ? ao_latency - free_frames : 0;
	if (ao_latency > 0) {
		unsigned fill = used * 100 / ao_latency;
		if (fill < ao_fill_min)
			ao_fill_min = fill;
		if (fill > ao_fill_max)
			ao_fill_max = fill;
	}
	error = ao_latency > 0 ? ((double) ao_latency / 2 - used) / ((double) ao_latency / 2) : 0.0;
	if (error > 1.0)
		error = 1.0;
	if (error < -1.0)
		error = -1.0;
	ratio = 1.0 + error * (error > 0.75 || error < -0.75 ? 0.02 : 0.005);
	ao_ratio_sum += ratio;
	ao_ratio_count++;
	step = 1.0 / ratio;
	for (p = ao_phase; p <= (double) (n - 1) && out < AO_OUT_MAX; p += step, out++) {
		int j = (int) (p < 0.0 ? -1 : p);
		double frac = p - j;
		for (c = 0; c < 2; c++) {
			int a = j < 0 ? ao_last[c] : data[j * 2 + c];
			int b = data[(j + 1 < (int) n ? j + 1 : (int) n - 1) * 2 + c];
			buf[out * 2 + c] = (int16_t) ((a + (b - a) * frac) * v / 100);
		}
	}
	ao_phase = p - n;
	if (ao_phase < -1.0)
		ao_phase = -1.0;
	ao_last[0] = data[(n - 1) * 2];
	ao_last[1] = data[(n - 1) * 2 + 1];
	if (out > free_frames)
		out = free_frames;
	if (out > 0)
		host_sound_write(buf, out);
	return frames;
}

static void core_audio_sample(int16_t left, int16_t right)
{
	int16_t frame[2] = {left, right};
	core_audio_batch(frame, 1);
}

static void sound_report(void)
{
	if (ao_open == 0 || ao_ratio_count == 0)
		return;
	LOG("sound: buffer %u%% to %u%% full, rate %+.2f%%\n", ao_fill_min, ao_fill_max,
	    (ao_ratio_sum / ao_ratio_count - 1.0) * 100.0);
	ao_ratio_sum = 0.0;
	ao_ratio_count = 0;
	ao_fill_min = ~0u;
	ao_fill_max = 0;
}

/* For the PC simulator's tests (tools/hostsim): sound frames the core has
   made since BMDOS started, and how many of them were not silence. */
void bmdos_sound_counts(unsigned long long *frames, unsigned long long *loud)
{
	*frames = ao_frames_total;
	*loud = ao_loud_total;
}

/* ---- Input ------------------------------------------------------------------- */

static retro_keyboard_event_t keyboard_cb;
static int open_menu;
static int pad_last_menu[HOST_PADS];
static int mouse_dx, mouse_dy, mouse_buttons;
static unsigned char key_down[RETROK_LAST];   /* RETROK codes held */

static void core_input_poll(void) {}

/* Gamepad PORT's buttons for the core's RetroPad: libretro's B (bottom),
   A (right), Y (left) and X (top) are where HOST_PAD_A, B, X and Y are.
   GPIO joystick 1 joins the first pad, joystick 2 the second. DOSBox Pure
   maps them (its "auto mapping" knows many games; otherwise its gamepad
   emulates a PC joystick, the stick its stick and A and B its buttons). */
/* Frames left of the first gamepad's L3 held for the core: its button for
   DOSBox Pure's own on-screen menu, which the menu's item for it presses
   (BMDOS's gamepads have no stick buttons). */
static int core_menu_frames;

void bmdos_open_core_menu(void)
{
	core_menu_frames = 3;
}

static int16_t joypad(unsigned port, unsigned id)
{
	int state = host_pad_state((int) port), gpio = port < 2 ? host_gpio_joystick((int) port) : 0;
	if (port == 0 && id == RETRO_DEVICE_ID_JOYPAD_L3)
		return core_menu_frames > 0;
	if (state < 0)
		state = 0;
	switch (id) {
	case RETRO_DEVICE_ID_JOYPAD_UP:
		return (state & HOST_PAD_UP) || (gpio & HOST_JOY_UP);
	case RETRO_DEVICE_ID_JOYPAD_DOWN:
		return (state & HOST_PAD_DOWN) || (gpio & HOST_JOY_DOWN);
	case RETRO_DEVICE_ID_JOYPAD_LEFT:
		return (state & HOST_PAD_LEFT) || (gpio & HOST_JOY_LEFT);
	case RETRO_DEVICE_ID_JOYPAD_RIGHT:
		return (state & HOST_PAD_RIGHT) || (gpio & HOST_JOY_RIGHT);
	case RETRO_DEVICE_ID_JOYPAD_B:
		return (state & HOST_PAD_A) || (gpio & HOST_JOY_FIRE);
	case RETRO_DEVICE_ID_JOYPAD_A:
		return (state & HOST_PAD_B) != 0;
	case RETRO_DEVICE_ID_JOYPAD_Y:
		return (state & HOST_PAD_X) != 0;
	case RETRO_DEVICE_ID_JOYPAD_X:
		return (state & HOST_PAD_Y) != 0;
	case RETRO_DEVICE_ID_JOYPAD_L:
		return (state & HOST_PAD_L) != 0;
	case RETRO_DEVICE_ID_JOYPAD_R:
		return (state & HOST_PAD_R) != 0;
	case RETRO_DEVICE_ID_JOYPAD_START:
		return (state & HOST_PAD_START) != 0;
	case RETRO_DEVICE_ID_JOYPAD_SELECT:
		return (state & HOST_PAD_SELECT) != 0;
	default:
		return 0;
	}
}

/* The left stick, -32767 to 32767 (the host's runs from 0 to 65535, about
   32767 at rest), with a dead zone. */
static int16_t analog(unsigned port, unsigned index, unsigned id)
{
	int v;
	if (index != RETRO_DEVICE_INDEX_ANALOG_LEFT)
		return 0;
	v = host_pad_axis((int) port, id == RETRO_DEVICE_ID_ANALOG_X ? 0 : 1);
	if (v < 0)
		return 0;
	v -= 32768;
	if (v > -2048 && v < 2048)
		v = 0;
	return (int16_t) (v < -32767 ? -32767 : v > 32767 ? 32767 : v);
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	device &= RETRO_DEVICE_MASK;
	if (device == RETRO_DEVICE_KEYBOARD)
		return id < RETROK_LAST ? key_down[id] : 0;
	if (device == RETRO_DEVICE_MOUSE && port == 0) {
		switch (id) {
		case RETRO_DEVICE_ID_MOUSE_X: return (int16_t) mouse_dx;
		case RETRO_DEVICE_ID_MOUSE_Y: return (int16_t) mouse_dy;
		case RETRO_DEVICE_ID_MOUSE_LEFT: return (mouse_buttons & 1) != 0;
		case RETRO_DEVICE_ID_MOUSE_RIGHT: return (mouse_buttons & 2) != 0;
		case RETRO_DEVICE_ID_MOUSE_MIDDLE: return (mouse_buttons & 4) != 0;
		default: return 0;
		}
	}
	if (port >= HOST_PADS)
		return 0;
	if (device == RETRO_DEVICE_JOYPAD) {
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
			int16_t mask = 0;
			unsigned b;
			for (b = 0; b < 16; b++)
				if (joypad(port, b))
					mask |= (int16_t) (1 << b);
			return mask;
		}
		return joypad(port, id);
	}
	if (device == RETRO_DEVICE_ANALOG) {
		if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
			return joypad(port, id) ? 32767 : 0;
		return analog(port, index, id);
	}
	return 0;
}

static void key_event(int usage, int pressed)
{
	unsigned code = bmdos_usage_to_retrok(usage);
	unsigned mods = host_key_modifiers(), m = 0;
	if (code == RETROK_UNKNOWN || code >= RETROK_LAST)
		return;
	key_down[code] = pressed != 0;
	if (mods & 0x11)
		m |= RETROKMOD_CTRL;
	if (mods & 0x22)
		m |= RETROKMOD_SHIFT;
	if (mods & 0x44)
		m |= RETROKMOD_ALT;
	if (mods & 0x88)
		m |= RETROKMOD_META;
	if (keyboard_cb != NULL)
		keyboard_cb(pressed != 0, code, 0, (uint16_t) m);
}

static void process_input(void)
{
	int usage, pressed, pad, dx, dy, buttons;
	/* The mouse moves since the last frame, as the core reads them. */
	mouse_dx = mouse_dy = 0;
	if (host_mouse_read(&dx, &dy, &buttons)) {
		double speed = host_option_int("mouse_speed", 100) / 100.0;
		mouse_dx = (int) (dx * speed);
		mouse_dy = (int) (dy * speed);
		mouse_buttons = buttons;
	}
	while (host_key_event(&usage, &pressed)) {
		/* The power key: Shut Down (the Pi 400 sends it for Fn+F10). */
		if (usage == USAGE_POWER) {
			if (pressed)
				bmdos_request_power(1);
			continue;
		}
		if (usage == KEY_MENU) {
			if (pressed)
				open_menu = TRUE;
			continue;
		}
		key_event(usage, pressed);
	}
	/* Gamepad Home, or Select and Start together, open BMDOS's menu
	   (DOSBox Pure's own on-screen menu and keyboard are on the pad's
	   L3 and R3, or Select). */
	for (pad = 0; pad < HOST_PADS; pad++) {
		int state = host_pad_state(pad);
		int menu;
		if (state < 0)
			state = 0;
		menu = (state & HOST_PAD_HOME)
		       || ((state & HOST_PAD_SELECT) && (state & HOST_PAD_START));
		if (menu && !pad_last_menu[pad])
			open_menu = TRUE;
		pad_last_menu[pad] = menu;
	}
}

/* Keys held when the menu opened are let go, so that none stick. */
static void release_keys(void)
{
	int usage;
	for (usage = 0; usage < 256; usage++) {
		unsigned code = bmdos_usage_to_retrok(usage);
		if (code != RETROK_UNKNOWN && code < RETROK_LAST && key_down[code])
			key_event(usage, 0);
	}
}

/* ---- The core's questions ------------------------------------------------------ */

static char system_dir[512];
static char save_dir[512];
static int core_ready;            /* retro_load_game() succeeded */
void bmdos_apply_pad_types(void);
static int core_shut_down;        /* RETRO_ENVIRONMENT_SHUTDOWN */

static void core_log(enum retro_log_level level, const char *fmt, ...)
{
	char text[1024];
	size_t n;
	va_list ap;
	if (level < RETRO_LOG_INFO && !option_on("core_debug", FALSE))
		return;
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	n = strlen(text);
	printf("[core] %s%s", text, n > 0 && text[n - 1] == '\n' ? "" : "\n");
}

static retro_time_t core_time_usec(void)
{
	return (retro_time_t) (bmdos_clock_now() * 1e6);
}

static retro_perf_tick_t core_perf_counter(void)
{
	return (retro_perf_tick_t) (bmdos_clock_now() * 1e9);
}

static uint64_t core_cpu_features(void)
{
#if defined(__aarch64__) || defined(__ARM_NEON)
	return RETRO_SIMD_NEON;
#else
	return 0;
#endif
}

static void perf_nothing(struct retro_perf_counter *counter) { (void) counter; }

/* A message the core shows ("Emulation Speed: 99%", what went wrong). */
static void core_message(const char *msg, int duration)
{
	char text[OSD_COLS + 1];
	if (msg == NULL)
		return;
	LOG("message: %s\n", msg);
	snprintf(text, sizeof text, "%s", msg);
	if (duration != 0)
		osd_notify(text);
}

static bool core_environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *) data)->log = core_log;
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *(const enum retro_pixel_format *) data == RETRO_PIXEL_FORMAT_XRGB8888;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char **) data = system_dir;
		return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char **) data = save_dir;
		return true;
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
		*(unsigned *) data = 2;
		return true;
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
		bmdos_options_define(data);
		return true;
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
		const struct retro_core_option_display *d = data;
		if (d != NULL && d->key != NULL)
			bmdos_option_set_visible(d->key, d->visible);
		return true;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *v = data;
		v->value = bmdos_option_get(v->key);
		return v->value != NULL;
	}
	case RETRO_ENVIRONMENT_SET_VARIABLE: {
		const struct retro_variable *v = data;
		if (v == NULL || v->key == NULL)
			return true;   /* "is this supported?" */
		return bmdos_option_set(v->key, v->value) != 0;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*(bool *) data = bmdos_options_changed() != 0;
		return true;
	case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
		keyboard_cb = ((const struct retro_keyboard_callback *) data)->callback;
		return true;
	case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
		struct retro_vfs_interface_info *info = data;
		if (info->required_interface_version > 3)
			return false;
		info->required_interface_version = 3;
		info->iface = (struct retro_vfs_interface *) bmdos_vfs();
		return true;
	}
	case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
		struct retro_perf_callback *p = data;
		memset(p, 0, sizeof *p);
		p->get_time_usec = core_time_usec;
		p->get_perf_counter = core_perf_counter;
		p->get_cpu_features = core_cpu_features;
		p->perf_register = perf_nothing;
		p->perf_start = perf_nothing;
		p->perf_stop = perf_nothing;
		return true;
	}
	case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
		/* One frame a display refresh. */
		struct retro_throttle_state *t = data;
		t->mode = RETRO_THROTTLE_VSYNC;
		t->rate = (float) refresh_rate;
		return true;
	}
	case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
		*(bool *) data = false;
		return true;
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
		take_geometry(&((const struct retro_system_av_info *) data)->geometry);
		return true;
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
		take_geometry(data);
		return true;
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
		const struct retro_message_ext *m = data;
		core_message(m->msg, (int) m->duration);
		return true;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE: {
		const struct retro_message *m = data;
		core_message(m->msg, (int) m->frames);
		return true;
	}
	case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
		*(unsigned *) data = 1;
		return true;
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
	case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
		return true;
	case RETRO_ENVIRONMENT_SHUTDOWN:
		/* DOS asked to switch the PC off: BMDOS starts it again. */
		LOG("the PC switched itself off\n");
		core_shut_down = TRUE;
		return true;
	default:
		/* No OpenGL (SET_HW_RENDER), MIDI device, disk control, network
		   packets: the core does without them. */
		return false;
	}
}

/* ---- Content -------------------------------------------------------------------- */

static char content[600];
static int restart_wanted;
static int content_gone;   /* its USB stick was pulled out: start without it */

const char *bmdos_content(void)
{
	return content;
}

void bmdos_load_content(const char *path)
{
	snprintf(content, sizeof content, "%s", path != NULL ? path : "");
	save_setting("content", content);
	restart_wanted = TRUE;
}

void bmdos_restart(void)
{
	restart_wanted = TRUE;
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

const char *bmdos_machine_text(void)
{
	static char text[96];
	if (content[0] == '\0')
		snprintf(text, sizeof text, "DOSBox Pure");
	else
		snprintf(text, sizeof text, "DOSBox Pure: %.60s", base_name(content));
	return text;
}

static void make_folder(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		mkdir(path, 0777);
}

/* ---- Gamepads ---------------------------------------------------------------- */

/* What each of the first two gamepads is to the PC (pad1_type, pad2_type in
   bmdos.cfg), as DOSBox Pure offers them (dosbox_pure_pad.h): its gamepad
   mapper (the game's own mapping where DOSBox Pure knows the game, else
   keyboard keys, and whatever is set in its Controller Mapper), or one of
   its presets. The preset's number is the libretro device's subclass. */
static const struct {
	const char *setting, *name;
	unsigned device;
} pad_types[] = {
	{"auto", "Auto (DOSBox Pure's mapper)", RETRO_DEVICE_JOYPAD},
	{"keyboard", "Keyboard Keys", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)},
	{"joystick1", "PC Joystick 1", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 4)},
	{"joystick2", "PC Joystick 2", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 5)},
	{"gravis", "Gravis GamePad", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 3)},
	{"thrustmaster", "Thrustmaster Flight Stick", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 6)},
	{"mouse", "Mouse (left stick)", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)},
	{"off", "Off", RETRO_DEVICE_NONE},
};
#define N_PAD_TYPES ((int) (sizeof pad_types / sizeof pad_types[0]))

int bmdos_pad_types(void) { return N_PAD_TYPES; }

const char *bmdos_pad_type_name(int type)
{
	return type >= 0 && type < N_PAD_TYPES ? pad_types[type].name : "";
}

int bmdos_pad_type(int pad)
{
	const char *v = host_option(pad == 0 ? "pad1_type" : "pad2_type", "auto");
	int i;
	for (i = 0; i < N_PAD_TYPES; i++)
		if (strcmp(v, pad_types[i].setting) == 0)
			return i;
	return 0;
}

void bmdos_apply_pad_types(void)
{
	int pad;
	if (!core_ready)
		return;
	for (pad = 0; pad < 2; pad++)
		retro_set_controller_port_device((unsigned) pad, pad_types[bmdos_pad_type(pad)].device);
}

void bmdos_set_pad_type(int pad, int type)
{
	if (type < 0 || type >= N_PAD_TYPES)
		return;
	save_setting(pad == 0 ? "pad1_type" : "pad2_type", pad_types[type].setting);
	bmdos_apply_pad_types();
}

static int started_once;          /* retro_init() has been called */

/* Starts the core with the content chosen; returns 0 if it couldn't. */
static int start_core(void)
{
	struct retro_game_info info;
	struct retro_system_av_info av;
	struct stat st;
	int ok;
	retro_set_environment(core_environment);
	retro_set_video_refresh(core_video);
	retro_set_audio_sample(core_audio_sample);
	retro_set_audio_sample_batch(core_audio_batch);
	retro_set_input_poll(core_input_poll);
	retro_set_input_state(core_input_state);
	if (!started_once) {
		retro_init();
		started_once = TRUE;
	}
	memset(&info, 0, sizeof info);
	info.path = content;
	core_shut_down = FALSE;
	if (content_gone) {
		/* Still kept as the content to start with (bmdos.cfg), for when
		   the stick is back and the Pi starts again. */
		content_gone = FALSE;
		LOG("%s was on the USB stick pulled out: starting without it\n", content);
		ok = retro_load_game(NULL);
	} else if (content[0] != '\0' && stat(content, &st) != 0) {
		char text[OSD_COLS + 1];
		LOG("%s is not there: starting without it\n", content);
		snprintf(text, sizeof text, "Not found: %s", base_name(content));
		osd_notify(text);
		ok = retro_load_game(NULL);
	} else if (content[0] != '\0') {
		LOG("starting %s\n", content);
		ok = retro_load_game(&info);
	} else {
		LOG("starting with no content\n");
		ok = retro_load_game(NULL);
	}
	if (!ok) {
		LOG("DOSBox Pure did not start\n");
		return FALSE;
	}
	core_ready = TRUE;
	bmdos_apply_pad_types();
	retro_get_system_av_info(&av);
	if (av.timing.sample_rate > 8000.0 && (unsigned) (av.timing.sample_rate + 0.5) != ao_rate) {
		ao_rate = (unsigned) (av.timing.sample_rate + 0.5);
		open_sound();
	}
	take_geometry(&av.geometry);
	LOG("DOSBox Pure running: %ux%u, %.2f Hz, sound %u Hz\n", av.geometry.base_width,
	    av.geometry.base_height, av.timing.fps, ao_rate);
	core_ready = TRUE;
	return TRUE;
}

/* Stops the PC (DOSBox Pure shuts DOSBox down, its thread ends); the core
   stays initialised, to be given content again. */
static void stop_core(void)
{
	retro_unload_game();
	core_ready = FALSE;
	keyboard_cb = NULL;
	memset(key_down, 0, sizeof key_down);
}

/* ---- Files ---------------------------------------------------------------- */

/* DOSBox Pure writes saved games and disk images straight to the card.
   The Pi can be switched off at any time: everything is pushed out to the
   card once a second. */
static void sync_files(void)
{
	int fd;
	fflush(NULL);
	for (fd = 3; fd < 64; fd++)
		fsync(fd);
}

/* ---- USB sticks ------------------------------------------------------------------ */

/* Once a frame: a USB stick pulled out (or swapped for another) that the
   content was on starts the PC again without it (DOSBox Pure can't lose
   its C: drive while it runs), as BMA800XL ejects its disks; a stick
   plugged in is said. */
static void watch_usb_sticks(void)
{
	static unsigned seen[HOST_USB_STICKS];
	char text[96];
	int i;
	for (i = 0; i < HOST_USB_STICKS; i++) {
		unsigned now = host_usb_stick(i);
		const char *root = host_usb_root(i);
		if (now == seen[i])
			continue;
		if (seen[i] != 0) {
			int on_it = root != NULL && content[0] != '\0'
			            && strncmp(content, root, strlen(root)) == 0;
			LOG("USB stick %d out%s\n", i + 1, on_it ? ": the PC's content was on it" : "");
			osd_notify(on_it ? "USB stick out: the PC's content was on it" : "USB stick out");
			if (on_it) {
				content_gone = TRUE;
				restart_wanted = TRUE;
			}
		}
		if (now != 0) {
			LOG("USB stick %d in: %s\n", i + 1, root ? root : "?");
			snprintf(text, sizeof text, "USB stick in: %.*s",
			         (int) strcspn(root ? root : "", "/"), root ? root : "");
			osd_notify(text);
		}
		seen[i] = now;
	}
}

/* ---- Requests from the web UI ------------------------------------------ */

static volatile int request_power = -1;   /* 0 reboot, 1 power off */
static volatile int request_coldstart = FALSE;
static char autostart_path[600];
static volatile int request_autostart = FALSE;

void bmdos_request_power(int power_off)
{
	request_power = power_off ? 1 : 0;
}

void bmdos_request_coldstart(void)
{
	request_coldstart = TRUE;
}

/* Called by the web UI ("start this file", a name kept from BMC64), on
   core 0: the emulator takes it up between frames. One at a time. */
void emu_autostart_interrupt(const char *path)
{
	if (request_autostart)
		return;
	snprintf(autostart_path, sizeof autostart_path, "%s", path);
	__sync_synchronize();
	request_autostart = TRUE;
}

int bmdos_pending_requests(void)
{
	return request_power >= 0 || request_coldstart || request_autostart;
}

static void power(int off)
{
	LOG("%s\n", off ? "shutting down" : "rebooting");
	sync_files();
	if (off)
		host_power_off();
	else
		host_reboot();
}

static void handle_requests(void)
{
	if (request_autostart) {
		__sync_synchronize();
		LOG("web UI: starting %s\n", autostart_path);
		bmdos_load_content(autostart_path);
		request_autostart = FALSE;
	}
	if (request_coldstart) {
		request_coldstart = FALSE;
		LOG("web UI: restart\n");
		restart_wanted = TRUE;
	}
	if (request_power >= 0) {
		int off = request_power;
		request_power = -1;
		power(off);
	}
}

static double last_sync_time;

void bmdos_idle(void)
{
	double now = host_time();
	feed_silence();
	if (now - last_sync_time >= 1.0) {
		last_sync_time = now;
		sync_files();
	}
	if (request_power >= 0)
		handle_requests();
}

/* ---- Speed, for the log ----------------------------------------------------- */

/* src/port/dosstatus.cpp: the PC's CPU core, cycles, clock and CS:IP. */
void bmdos_dosbox_status(char *text, size_t size);

static unsigned speed_frames;
static double speed_since, speed_core;
static int speed_percent = 100, core_percent;

void bmdos_status_text(char *text, size_t size)
{
	unsigned mhz, celsius;
	host_cpu_status(&mhz, &celsius);
	snprintf(text, size, "Frames %d%% of the display's %.2f Hz, DOSBox %d%% of the time\n"
	         "ARM %u MHz, %u C", speed_percent, refresh_rate, core_percent, mhz, celsius);
}

/* Every 10 s: how many frames were shown against the display's refresh,
   how much of the time the core took, the ARM's clock and temperature, and
   where the rest went (the kernel's three parts of showing a frame). */
static void report_speed(double now)
{
	double span = now - speed_since, tasks, log, picture;
	unsigned mhz, celsius;
	if (span <= 0.0)
		return;
	speed_percent = (int) (speed_frames * 100.0 / (span * refresh_rate) + 0.5);
	core_percent = (int) (speed_core * 100.0 / span + 0.5);
	host_cpu_status(&mhz, &celsius);
	host_video_times(&tasks, &log, &picture);
	LOG("speed %d%% (%u frames in %.1f s), ARM %u MHz %u C; DOSBox %d%%, "
	    "frames: tasks %.0f%% log %.0f%% picture %.0f%%\n",
	    speed_percent, speed_frames, span, mhz, celsius, core_percent,
	    tasks * 100.0 / span, log * 100.0 / span, picture * 100.0 / span);
	if (core_ready) {
		char pc[160];
		bmdos_dosbox_status(pc, sizeof pc);
		LOG("PC: %s\n", pc);
	}
	sound_report();
	speed_since = now;
	speed_frames = 0;
	speed_core = 0.0;
}

/* ---- Main loop ---------------------------------------------------------------- */

static void show_network_address(void)
{
	static int shown;
	char address[32];
	if (shown || !option_on("webui", TRUE))
		return;
	if (host_network_address(address, sizeof(address))) {
		char text[64];
		snprintf(text, sizeof(text), "Web UI: http://%s/", address);
		osd_notify(text);
		shown = TRUE;
	}
}

/* DOSBox Pure would not start: says so until the menu is opened (to choose
   something else) or the web UI asks for something. */
static void not_started(void)
{
	int usage, pressed;
	osd_open();
	osd_message("DOSBox Pure did not start. The log (see the User Guide) says why. "
	            "F12 opens the menu.", FALSE);
	for (;;) {
		bmdos_idle();
		host_video_present(TRUE);
		while (host_key_event(&usage, &pressed))
			if (pressed && usage == KEY_MENU) {
				osd_close();
				bmdos_menu();
				return;
			}
		if (bmdos_pending_requests()) {
			osd_close();
			handle_requests();
			return;
		}
	}
}

static void show_frame(void)
{
	if (osd_view)
		return;
	if (depth == 32)
		osd_draw_notification(fb32_pixels, fb32_pitch_px, 32, pic_w, pic_h);
	else
		osd_draw_notification(fb_pixels, fb_pitch_px, 16, pic_w, pic_h);
	host_video_present(TRUE);
}

void bmdos_main(void)
{
	int pitch, pitch32 = 0;
	double last_second;

	if (host_video_init(FB_W, FB_H, &fb_pixels, &pitch) != 0) {
		LOG("no display\n");
		for (;;)
			host_sleep_us(1000000);
	}
	fb_pitch_px = pitch / 2;
	fb32_pixels = host_video_init32(&pitch32);
	fb32_pitch_px = pitch32 / 4;
	clear_framebuffer();
	choose_depth();
	apply_view();
	host_display_match_rate(60.0);
	measure_refresh_rate();
	open_sound();
	feed_silence();

	/* DOSBox Pure's folders on the card. */
	bmdos_card_path(system_dir, sizeof system_dir, "dos", "");
	make_folder(system_dir);
	bmdos_card_path(system_dir, sizeof system_dir, "dos", "system");
	make_folder(system_dir);
	bmdos_card_path(save_dir, sizeof save_dir, "dos", "saves");
	make_folder(save_dir);
	snprintf(content, sizeof content, "%s", host_option("content", ""));
	LOG("version %s; system folder %s, saves %s\n", BMDOS_VERSION, system_dir, save_dir);

	last_second = speed_since = host_time();
	for (;;) {
		static int speed_seconds;
		double now, core_start;

		if (!core_ready) {
			restart_wanted = FALSE;
			if (!start_core()) {
				not_started();
				continue;
			}
		}
		if (restart_wanted || core_shut_down) {
			restart_wanted = FALSE;
			LOG("starting the PC again\n");
			stop_core();
			continue;
		}

		process_input();
		if (open_menu) {
			open_menu = FALSE;
			release_keys();
			bmdos_menu();
			release_keys();
			apply_view();
			if (restart_wanted)
				continue;
		}

		audio_this_frame = 0;
		core_start = host_time();
		retro_run();
		/* The core's other threads run at least once a frame. */
		bma_threads_yield();
		if (core_menu_frames > 0)
			core_menu_frames--;
		/* FujiNet-PC's connection, and its COM port in a DOSBox that has
		   just set up its own (fujinet.cpp). */
		bmdos_fujinet_frame();
		/* IRQ 13 to 15's vectors, which DOSBox Pure's BIOS leaves empty
		   (biosfix.cpp). */
		bmdos_biosfix_frame();
		speed_core += host_time() - core_start;
		speed_frames++;
		/* Frames without sound (the PC starting): silence instead, so
		   that the output doesn't run dry. */
		if (audio_this_frame == 0)
			feed_silence();
		watch_usb_sticks();

		show_frame();

		now = host_time();
		if (now - last_second >= 1.0) {
			last_second = now;
			sync_files();
			show_network_address();
			if (++speed_seconds >= 10) {
				speed_seconds = 0;
				report_speed(now);
			}
		}
		handle_requests();
	}
}
