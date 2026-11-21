/*
 * bmdos.h - what BMDOS's front end files (src/port) share.
 *
 * platform.c runs DOSBox Pure, a libretro core linked in, a frame at a
 * time; coreopts.c keeps the core's settings, vfs.c its files, keys.c its
 * keys; menu.c and osd.c are BMDOS's own menu.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMDOS_BMDOS_H_
#define BMDOS_BMDOS_H_

#include <stddef.h>
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define LOG(...) do { printf("BMDOS: " __VA_ARGS__); } while (0)

/* USB HID usages the menu uses. */
enum {
	USAGE_A = 0x04,
	USAGE_RETURN = 0x28, USAGE_ESCAPE = 0x29, USAGE_BACKSPACE = 0x2a,
	USAGE_TAB = 0x2b, USAGE_SPACE = 0x2c,
	USAGE_F1 = 0x3a, USAGE_F4 = 0x3d, USAGE_F10 = 0x43, USAGE_F11 = 0x44,
	USAGE_F12 = 0x45,
	USAGE_PAUSE = 0x48, USAGE_HOME = 0x4a, USAGE_PAGEUP = 0x4b,
	USAGE_DELETE = 0x4c, USAGE_END = 0x4d, USAGE_PAGEDOWN = 0x4e,
	USAGE_RIGHT = 0x4f, USAGE_LEFT = 0x50, USAGE_DOWN = 0x51, USAGE_UP = 0x52,
	USAGE_KP_ENTER = 0x58, USAGE_POWER = 0x66,
	USAGE_LCTRL = 0xe0, USAGE_LSHIFT = 0xe1, USAGE_LALT = 0xe2,
	USAGE_RCTRL = 0xe4, USAGE_RSHIFT = 0xe5, USAGE_RALT = 0xe6
};

/* The key BMDOS keeps for itself: F12 opens the menu. */
#define KEY_MENU USAGE_F12

/* ---- platform.c ---------------------------------------------------------- */

/* The SD card's top folder ("SD:/" on the Pi, the current folder in the PC
   simulator) and a path in it. */
const char *bmdos_sd_root(void);
void bmdos_card_path(char *path, unsigned size, const char *dir, const char *name);

/* The display's frame buffer (RGB565, 640x480 at least), for the menu. */
uint16_t *bmdos_framebuffer(int *pitch_px);
/* The menu on (the frame buffer shows it) or off (the PC's picture). */
void bmdos_osd_view(int on);
void bmdos_video_settings_changed(void);
void bmdos_display_mode_changed(void);
double bmdos_refresh_rate(void);

/* What the PC runs: a ZIP, a program, a disk image or a folder, or "" for
   none (DOSBox Pure's start menu, or its prompt). */
const char *bmdos_content(void);
/* Starts the PC again with PATH ("" for none), and saves it as the one to
   start with. */
void bmdos_load_content(const char *path);
/* Starts the PC again with the same content (a cold start). */
void bmdos_restart(void);
/* What gamepad PAD (0 or 1) is to the PC: one of bmdos_pad_types(). */
int bmdos_pad_types(void);
const char *bmdos_pad_type_name(int type);
int bmdos_pad_type(int pad);
void bmdos_set_pad_type(int pad, int type);
void bmdos_apply_pad_types(void);
/* Opens DOSBox Pure's own on-screen menu (its L3 button). */
void bmdos_open_core_menu(void);
/* Sound volume, 0 to 100. */
int bmdos_volume(void);
void bmdos_set_volume(int v);
/* The text the web UI shows for the machine. */
const char *bmdos_machine_text(void);
/* Speed figures for the menu's Information page. */
void bmdos_status_text(char *text, size_t size);

/* Requests (the web UI's, the menu's), acted on between frames. */
void bmdos_request_power(int power_off);
void bmdos_request_coldstart(void);
int bmdos_pending_requests(void);
/* Keeps the sound fed, files synced and requests answered while the menu
   is open. */
void bmdos_idle(void);

/* ---- menu.c -------------------------------------------------------------- */

void bmdos_menu(void);

/* ---- coreopts.c ---------------------------------------------------------- */

/* DOSBox Pure's settings (core options), as it describes them
   (RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2), with their values from
   bmdos.cfg ("dosbox_pure_cycles=max" lines) or their defaults. */
struct retro_core_options_v2;
void bmdos_options_define(const struct retro_core_options_v2 *options);
int bmdos_options_count(void);
/* Option I's key, description and category ("" for none). */
const char *bmdos_option_key(int i);
const char *bmdos_option_desc(int i);
const char *bmdos_option_category(int i);
const char *bmdos_option_info(int i);
/* Option I's possible values, as value and label, up to N. */
int bmdos_option_values(int i, const char **values, const char **labels, int n);
/* Category I's key and description, of bmdos_categories_count(). */
int bmdos_categories_count(void);
const char *bmdos_category_key(int i);
const char *bmdos_category_desc(int i);
/* The value of KEY now (NULL if there is no such option). */
const char *bmdos_option_get(const char *key);
/* Sets KEY to VALUE and saves it; returns 0 if there is no such option. */
int bmdos_option_set(const char *key, const char *value);
/* Whether the core has hidden option I (SET_CORE_OPTIONS_DISPLAY). */
int bmdos_option_visible(int i);
void bmdos_option_set_visible(const char *key, int visible);
/* Set when an option changed (GET_VARIABLE_UPDATE); cleared by reading. */
int bmdos_options_changed(void);
/* BMDOS's own choice of default for KEY over the core's, or NULL. */
const char *bmdos_option_bmdos_default(const char *key);

/* ---- fujinet.cpp --------------------------------------------------------- */

/* A FujiNet on COM1 or COM2 through FujiNet-PC's RS-232 build (fujinet=1). */
int bmdos_fujinet_enabled(void);
void bmdos_fujinet_frame(void);
void bmdos_fujinet_status(char *text, unsigned size);

/* ---- keys.c ---------------------------------------------------------------- */

/* A USB HID usage as a libretro key (RETROK_*), RETROK_UNKNOWN if none. */
unsigned bmdos_usage_to_retrok(int usage);

/* ---- vfs.c ----------------------------------------------------------------- */

/* The libretro file system interface (RETRO_ENVIRONMENT_GET_VFS_INTERFACE,
   version 3) over the C library's files: the SD card and USB sticks. */
struct retro_vfs_interface;
const struct retro_vfs_interface *bmdos_vfs(void);

/* ---- clock.c ---------------------------------------------------------------- */

/* Seconds the core spent sleeping (its own waits), for the load figures. */
extern double bmdos_clock_slept;

#endif /* BMDOS_BMDOS_H_ */
