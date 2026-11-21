/*
 * menu.c - the BMDOS menu.
 *
 * F12, a gamepad's Home button or Select+Start open it. What the PC runs
 * (a game's ZIP, a program, a disk image, chosen from the card or a USB
 * stick), DOSBox Pure's own settings, and BMDOS's (video, sound, network,
 * power). DOSBox Pure has its own menu as well, in the picture: its start
 * menu, which lists the programs in a ZIP and boots a disk image, and its
 * on-screen keyboard and gamepad mapper (on the gamepad's L3/R3 or Select,
 * or Ctrl+Home on the keyboard).
 *
 * DOSBox Pure's settings are listed as the core describes them
 * (coreopts.c), in its own categories; a change takes effect the way it
 * does in RetroArch (most at once, some at the PC's next start, which the
 * setting's description says). BMDOS's are kept in bmdos.cfg, as the
 * core's are.
 *
 * From BMCoco's, BMApple2's, BMAtariST's and BMNeXT's menu.c.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "../host.h"
#include "bmdos.h"
#include "osd.h"

#ifndef BMDOS_VERSION
#define BMDOS_VERSION "dev"
#endif
#ifndef DOSBOX_PURE_VERSION
#define DOSBOX_PURE_VERSION "?"
#endif

#define WPA_CONFIG "wpa_supplicant.conf"
/* Values a setting can have, at most (libretro's RETRO_NUM_CORE_OPTION_VALUES_MAX). */
#define RETRO_VALUES 128

/* Something changed that only takes effect after a reboot. */
static int reboot_needed;
/* The menu should close (the PC starts again). */
static int leave_menu;

/* Where the file browser was last ("" until it's first used). */
static char browse_dir[256];

/* ---- Settings ---------------------------------------------------------- */

static void save_setting(const char *name, const char *value)
{
	if (!host_option_set(name, value) || !host_options_save())
		osd_message("Cannot save bmdos.cfg", TRUE);
}

static void save_setting_int(const char *name, int value)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", value);
	save_setting(name, text);
}

static int setting_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "yes") == 0
	    || strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

static unsigned check(int on)
{
	return OSD_ITEM_CHECK | (on ? OSD_ITEM_CHECKED : 0);
}

static void power(int off)
{
	bmdos_request_power(off);
	bmdos_idle();
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

/* ---- What the PC runs ------------------------------------------------------ */

/* What DOSBox Pure can start (its dosbox_pure_libretro.info): a game in a
   ZIP (or .dosz, a ZIP by another name) or a folder's program, a DOS
   program or batch file, a CD image, a floppy or hard disk image, a list
   of disks (.m3u), a DOSBox configuration. */
static const char *const content_exts[] = {
	"zip", "dosz", "exe", "com", "bat", "iso", "cue", "chd", "ins", "img", "ima",
	"vhd", "jrc", "tc", "m3u", "m3u8", "conf", NULL
};

static void choose_content(void)
{
	char path[600];
	if (browse_dir[0] == '\0') {
		const char *current = bmdos_content();
		if (current[0] != '\0') {
			snprintf(browse_dir, sizeof browse_dir, "%s", current);
			*(char *) base_name(browse_dir) = '\0';
		} else
			bmdos_card_path(browse_dir, sizeof browse_dir, "dos", "");
	}
	if (!osd_browse("Run a Game or Program", browse_dir, sizeof browse_dir, content_exts,
	                path, sizeof path))
		return;
	bmdos_load_content(path);
	leave_menu = TRUE;
}

/* ---- DOSBox Pure's settings ---------------------------------------------- */

/* The label of option I's value now, or the value itself. */
static const char *value_label(int i)
{
	static const char *values[RETRO_VALUES], *labels[RETRO_VALUES];
	const char *now = bmdos_option_get(bmdos_option_key(i));
	int n = bmdos_option_values(i, values, labels, RETRO_VALUES), k;
	for (k = 0; k < n; k++)
		if (now != NULL && strcmp(values[k], now) == 0)
			return labels[k];
	return now != NULL ? now : "";
}

static void choose_value(int i)
{
	static const char *values[RETRO_VALUES + 1], *labels[RETRO_VALUES + 1];
	const char *now = bmdos_option_get(bmdos_option_key(i));
	int n = bmdos_option_values(i, values, labels, RETRO_VALUES), k, current = 0;
	for (k = 0; k < n; k++)
		if (now != NULL && strcmp(values[k], now) == 0)
			current = k;
	labels[n] = NULL;
	k = osd_choose(bmdos_option_desc(i), labels, current);
	if (k >= 0 && k != current)
		bmdos_option_set(bmdos_option_key(i), values[k]);
}

/* The settings of one category (KEY, "" for those in none). */
static void category_menu(const char *key, const char *title)
{
	enum { MAX_ITEMS = 64 };
	struct osd_item items[MAX_ITEMS];
	int index[MAX_ITEMS];
	int sel = 0;
	for (;;) {
		int n = 0, i, chosen;
		for (i = 0; i < bmdos_options_count() && n < MAX_ITEMS; i++) {
			if (strcmp(bmdos_option_category(i), key) != 0 || !bmdos_option_visible(i))
				continue;
			items[n] = (struct osd_item){bmdos_option_desc(i), value_label(i),
			                             OSD_ITEM_SUBMENU, n};
			index[n] = i;
			n++;
		}
		if (n == 0) {
			osd_message("No settings here now.", TRUE);
			return;
		}
		chosen = osd_menu(title, items, n, &sel);
		if (chosen < 0 || osd_closing())
			return;
		choose_value(index[chosen]);
	}
}

static void dosbox_menu(void)
{
	enum { MAX_ITEMS = 24 };
	struct osd_item items[MAX_ITEMS];
	int sel = 0;
	for (;;) {
		int n = 0, i, chosen, other = 0;
		for (i = 0; i < bmdos_categories_count() && n < MAX_ITEMS - 4; i++)
			items[n++] = (struct osd_item){bmdos_category_desc(i), NULL, OSD_ITEM_SUBMENU, i};
		for (i = 0; i < bmdos_options_count(); i++)
			if (bmdos_option_category(i)[0] == '\0' && bmdos_option_visible(i))
				other++;
		if (other > 0)
			items[n++] = (struct osd_item){"Other", NULL, OSD_ITEM_SUBMENU, 1000};
		items[n++] = (struct osd_item){"", NULL, OSD_ITEM_LABEL, -1};
		items[n++] = (struct osd_item){"Most take effect at once; the rest when the", NULL,
		                               OSD_ITEM_LABEL, -1};
		items[n++] = (struct osd_item){"PC starts again (Restart the PC).", NULL,
		                               OSD_ITEM_LABEL, -1};
		if (bmdos_options_count() == 0) {
			osd_message("DOSBox Pure has not started.", TRUE);
			return;
		}
		chosen = osd_menu("DOSBox Settings", items, n, &sel);
		if (chosen < 0 || osd_closing())
			return;
		if (chosen == 1000)
			category_menu("", "Other");
		else
			category_menu(bmdos_category_key(chosen), bmdos_category_desc(chosen));
	}
}

/* ---- Gamepads ------------------------------------------------------------ */

static void gamepads_menu(void)
{
	static const char *names[16];
	int sel = 0, n = bmdos_pad_types(), i;
	for (i = 0; i < n && i < 15; i++)
		names[i] = bmdos_pad_type_name(i);
	names[i] = NULL;
	for (;;) {
		struct osd_item items[] = {
			{"Gamepad 1", bmdos_pad_type_name(bmdos_pad_type(0)), OSD_ITEM_SUBMENU, 0},
			{"Gamepad 2", bmdos_pad_type_name(bmdos_pad_type(1)), OSD_ITEM_SUBMENU, 1},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Auto: the game's own mapping where", NULL, OSD_ITEM_LABEL, -1},
			{"DOSBox Pure knows the game, else keys;", NULL, OSD_ITEM_LABEL, -1},
			{"change it in its Controller Mapper", NULL, OSD_ITEM_LABEL, -1},
			{"(its start menu's right side). A game", NULL, OSD_ITEM_LABEL, -1},
			{"that wants a joystick: PC Joystick 1.", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen = osd_menu("Gamepads", items, sizeof items / sizeof items[0], &sel);
		if (chosen < 0 || osd_closing())
			return;
		i = osd_choose(chosen == 0 ? "Gamepad 1" : "Gamepad 2", names, bmdos_pad_type(chosen));
		if (i >= 0)
			bmdos_set_pad_type(chosen, i);
	}
}

/* ---- Video -------------------------------------------------------------- */

/* The picture's height as a percentage of the screen's (the scale option):
   less for a TV that cuts off the edges (overscan). From BMCoco. */
static int screen_size(void)
{
	double scale = host_option_double("scale", 1.0);
	if (scale <= 0.1 || scale > 1.0)
		scale = 1.0;
	return (int) (scale * 100.0 + 0.5);
}

static void choose_screen_size(void)
{
	static const char *const choices[] = {
		"100%: fill the screen", "95%", "90%", "85%", "80%", NULL
	};
	static const int sizes[] = {100, 95, 90, 85, 80};
	int size = screen_size(), current = 0, k, i;
	char text[16];
	for (k = 1; k < 5; k++) {
		int d = sizes[k] - size, best = sizes[current] - size;
		if ((d < 0 ? -d : d) < (best < 0 ? -best : best))
			current = k;
	}
	i = osd_choose("Screen Size", choices, current);
	if (i < 0)
		return;
	snprintf(text, sizeof text, "%d.%02d", sizes[i] / 100, sizes[i] % 100);
	save_setting("scale", text);
	bmdos_video_settings_changed();
}

static void video_menu(void)
{
	enum { V_SIZE, V_RESOLUTION, V_SHADER, V_CURVATURE, V_SMOOTH };
	/* The Pi 5 only: its firmware ignores hdmi_mode (from BMA800XL). */
	static const char *const resolutions[] = {"auto", "1920x1080", "1280x720", NULL};
	static const char *const resolution_names[] = {"Auto", "1920x1080", "1280x720", NULL};
	int sel = 0;
	for (;;) {
		const char *res = host_option("resolution", "auto");
		int res_index = 0, n_items, k;
		char size_text[8];
		snprintf(size_text, sizeof size_text, "%d%%", screen_size());
		for (k = 0; resolutions[k] != NULL; k++)
			if (strcmp(res, resolutions[k]) == 0)
				res_index = k;
		struct osd_item items[] = {
			{"Screen Size", size_text, OSD_ITEM_SUBMENU, V_SIZE},
			{"Resolution", resolution_names[res_index], OSD_ITEM_SUBMENU, V_RESOLUTION},
			{"CRT Effect (reboot)", NULL, check(setting_on("crt_shader", FALSE)), V_SHADER},
			{"CRT Curvature (reboot)", NULL, check(setting_on("crt_curvature", FALSE)), V_CURVATURE},
			{"Smooth Scaling (reboot)", NULL, check(setting_on("smooth", FALSE)), V_SMOOTH},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"If your TV cuts off the edges, make", NULL, OSD_ITEM_LABEL, -1},
			{"the Screen Size smaller. The PC's own", NULL, OSD_ITEM_LABEL, -1},
			{"video settings are in DOSBox Settings.", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen, i;
		n_items = sizeof(items) / sizeof(items[0]);
		if (!host_display_resolution_choice()) {
			/* Resolution is only for the Pi 5 (with video_driver=kms). */
			for (k = 1; k < n_items - 1; k++)
				items[k] = items[k + 1];
			n_items--;
		}
		chosen = osd_menu("Video", items, n_items, &sel);
		switch (chosen) {
		case V_SIZE:
			choose_screen_size();
			break;
		case V_RESOLUTION:
			i = osd_choose("Resolution", resolution_names, res_index);
			if (i >= 0 && i != res_index) {
				save_setting("resolution", resolutions[i]);
				bmdos_display_mode_changed();
			}
			break;
		case V_CURVATURE:
			save_setting("crt_curvature", setting_on("crt_curvature", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SHADER:
			save_setting("crt_shader", setting_on("crt_shader", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SMOOTH:
			save_setting("smooth", setting_on("smooth", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		default:
			return;
		}
	}
}

/* ---- Sound ---------------------------------------------------------------- */

static void sound_menu(void)
{
	enum { S_VOLUME, S_OUTPUT };
	static const char *const volumes[] = {
		"100%", "90%", "80%", "70%", "60%", "50%", "40%", "30%", "20%", "10%", "Off", NULL
	};
	static const char *const outputs[] = {"auto", "hdmi", "analog"};
	static const char *const output_names[] = {"Automatic", "HDMI", "Headphone jack", NULL};
	int sel = 0;
	for (;;) {
		const char *out = host_option("audio_out", "auto");
		int out_index = strcmp(out, "hdmi") == 0 ? 1
		                : strcmp(out, "analog") == 0 || strcmp(out, "headphones") == 0 ? 2 : 0;
		char volume[8];
		struct osd_item items[] = {
			{"Volume", volume, OSD_ITEM_SUBMENU, S_VOLUME},
			{"Output (reboot)", output_names[out_index], OSD_ITEM_SUBMENU, S_OUTPUT},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"The PC's sound cards are in DOSBox", NULL, OSD_ITEM_LABEL, -1},
			{"Settings, Audio.", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen, i;
		if (bmdos_volume() == 0)
			snprintf(volume, sizeof volume, "Off");
		else
			snprintf(volume, sizeof volume, "%d%%", bmdos_volume());
		chosen = osd_menu("Sound", items, sizeof(items) / sizeof(items[0]), &sel);
		if (chosen == S_VOLUME) {
			i = osd_choose("Volume", volumes, (100 - bmdos_volume() + 5) / 10);
			if (i >= 0)
				bmdos_set_volume(100 - i * 10);
			continue;
		}
		if (chosen != S_OUTPUT)
			return;
		i = osd_choose("Sound Output", output_names, out_index);
		if (i >= 0 && i != out_index) {
			save_setting("audio_out", outputs[i]);
			reboot_needed = TRUE;
		}
	}
}

/* ---- Network --------------------------------------------------------------- */

static char wifi_ssid[33], wifi_psk[64], wifi_country[3];

static void wpa_path(char *path, size_t size)
{
	bmdos_card_path(path, (unsigned) size, "", WPA_CONFIG);
}

static void copy_quoted(char *dst, size_t size, const char *src)
{
	const char *end;
	if (*src == '"')
		src++;
	end = strchr(src, '"');
	if (end == NULL)
		end = src + strcspn(src, "\r\n");
	if ((size_t) (end - src) >= size)
		end = src + size - 1;
	memcpy(dst, src, end - src);
	dst[end - src] = '\0';
}

static void read_wifi_config(void)
{
	char path[256], line[160];
	FILE *f;
	wifi_ssid[0] = wifi_psk[0] = '\0';
	strcpy(wifi_country, "US");
	wpa_path(path, sizeof(path));
	f = fopen(path, "r");
	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *p = line + strspn(line, " \t");
		if (strncmp(p, "country=", 8) == 0)
			copy_quoted(wifi_country, sizeof(wifi_country), p + 8);
		else if (strncmp(p, "ssid=", 5) == 0 && wifi_ssid[0] == '\0')
			copy_quoted(wifi_ssid, sizeof(wifi_ssid), p + 5);
		else if (strncmp(p, "psk=", 4) == 0 && wifi_psk[0] == '\0')
			copy_quoted(wifi_psk, sizeof(wifi_psk), p + 4);
	}
	fclose(f);
}

static int write_wifi_config(void)
{
	char path[256];
	FILE *f;
	wpa_path(path, sizeof(path));
	f = fopen(path, "w");
	if (f == NULL)
		return FALSE;
	fprintf(f, "country=%s\n\nnetwork={\n\tssid=\"%s\"\n", wifi_country, wifi_ssid);
	if (wifi_psk[0] != '\0')
		fprintf(f, "\tpsk=\"%s\"\n\tkey_mgmt=WPA-PSK\n", wifi_psk);
	else
		fprintf(f, "\tkey_mgmt=NONE\n");
	fprintf(f, "}\n");
	return fclose(f) == 0;
}

static void edit_wifi(const char *title, char *value, size_t size, int secret)
{
	char text[64];
	snprintf(text, sizeof(text), "%s", value);
	if (osd_edit(title, text, size < sizeof(text) ? size : sizeof(text), secret)) {
		snprintf(value, size, "%s", text);
		if (write_wifi_config())
			reboot_needed = TRUE;
		else
			osd_message("Cannot write " WPA_CONFIG, TRUE);
	}
}

static int compare_signal(const void *a, const void *b)
{
	return ((const struct host_wifi_ap *) b)->signal - ((const struct host_wifi_ap *) a)->signal;
}

/* Returns 1 if a network was chosen, 2 if it also needs a password. */
static int scan_wifi(void)
{
	enum { MAX_APS = 20 };
	static struct host_wifi_ap aps[MAX_APS];
	static char signal[MAX_APS][16];
	struct osd_item items[MAX_APS];
	int n, i, sel = 0, chosen;

	osd_message("Scanning for Wi-Fi networks...", FALSE);
	n = host_wifi_scan(aps, MAX_APS);
	if (n < 0) {
		if (osd_confirm("Wi-Fi is Off", "Switch to Wi-Fi and reboot?")) {
			save_setting("network", "wifi");
			power(0);
		}
		return 0;
	}
	if (n == 0) {
		osd_message("No Wi-Fi networks found.", TRUE);
		return 0;
	}
	qsort(aps, n, sizeof(aps[0]), compare_signal);
	for (i = 0; i < n; i++) {
		snprintf(signal[i], sizeof(signal[i]), "%s%d", aps[i].secure ? "* " : "", aps[i].signal);
		items[i] = (struct osd_item){aps[i].ssid, signal[i], 0, i};
	}
	chosen = osd_menu("Choose a Wi-Fi Network", items, n, &sel);
	if (chosen < 0)
		return 0;
	if (!aps[chosen].secure || strcmp(wifi_ssid, aps[chosen].ssid) != 0)
		wifi_psk[0] = '\0';
	snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", aps[chosen].ssid);
	if (!write_wifi_config()) {
		osd_message("Cannot write " WPA_CONFIG, TRUE);
		return 0;
	}
	save_setting("network", "wifi");
	reboot_needed = TRUE;
	return aps[chosen].secure && wifi_psk[0] == '\0' ? 2 : 1;
}

static void network_menu(void)
{
	enum { N_DEVICE, N_SCAN, N_SSID, N_PASSWORD, N_COUNTRY, N_WEBUI, N_PIN, N_TIMEZONE, N_REBOOT };
	static const char *const devices[] = {"off", "ethernet", "wifi", NULL};
	static const char *const device_names[] = {"Off", "Ethernet", "Wi-Fi", NULL};
	int sel = 2;
	for (;;) {
		char status[64], tz[16];
		const char *net = host_option("network", "off");
		const char *pin = host_option("webui_pin", "");
		int device = strcmp(net, "ethernet") == 0 ? 1 : strcmp(net, "wifi") == 0 ? 2 : 0;
		int minutes = host_option_int("timezone", 0);
		int chosen, i;

		read_wifi_config();
		host_network_status(status, sizeof(status));
		if (minutes == 0)
			strcpy(tz, "UTC");
		else
			snprintf(tz, sizeof(tz), "UTC%+d", minutes / 60);
		{
			struct osd_item items[] = {
				{status, NULL, OSD_ITEM_LABEL, -1},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Network", device_names[device], OSD_ITEM_SUBMENU, N_DEVICE},
				{"Scan for Wi-Fi Networks", NULL, OSD_ITEM_SUBMENU, N_SCAN},
				{"Wi-Fi Network", wifi_ssid[0] ? wifi_ssid : "(none)", OSD_ITEM_SUBMENU, N_SSID},
				{"Wi-Fi Password", wifi_psk[0] ? "********" : "(none)", OSD_ITEM_SUBMENU, N_PASSWORD},
				{"Wi-Fi Country", wifi_country, OSD_ITEM_SUBMENU, N_COUNTRY},
				{"Web UI", NULL, check(setting_on("webui", TRUE)), N_WEBUI},
				{"Web UI PIN", pin[0] ? pin : "(none)", OSD_ITEM_SUBMENU, N_PIN},
				{"Time Zone", tz, OSD_ITEM_SUBMENU, N_TIMEZONE},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Changes apply after a reboot.", NULL, OSD_ITEM_LABEL, -1},
				{"Reboot Now", NULL, 0, N_REBOOT},
			};
			chosen = osd_menu("Network", items, sizeof(items) / sizeof(items[0]), &sel);
		}
		switch (chosen) {
		case N_DEVICE:
			i = osd_choose("Network", device_names, device);
			if (i >= 0 && i != device) {
				save_setting("network", devices[i]);
				reboot_needed = TRUE;
			}
			break;
		case N_SCAN:
			if (scan_wifi() == 2) {
				wifi_psk[0] = '\0';
				edit_wifi("Wi-Fi Password", wifi_psk, sizeof(wifi_psk), TRUE);
			}
			break;
		case N_SSID:
			edit_wifi("Wi-Fi Network Name", wifi_ssid, sizeof(wifi_ssid), FALSE);
			break;
		case N_PASSWORD:
			wifi_psk[0] = '\0';
			edit_wifi("Wi-Fi Password (blank: none)", wifi_psk, sizeof(wifi_psk), TRUE);
			break;
		case N_COUNTRY:
			edit_wifi("Two Letter Country Code", wifi_country, sizeof(wifi_country), FALSE);
			break;
		case N_WEBUI:
			save_setting("webui", setting_on("webui", TRUE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case N_PIN: {
			char text[9];
			snprintf(text, sizeof(text), "%s", pin);
			if (osd_edit("Web UI PIN (blank: none)", text, sizeof(text), FALSE)) {
				save_setting("webui_pin", text);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_TIMEZONE: {
			static char labels[27][8];
			static const char *zones[28];
			int h, current = 12;
			for (h = -12; h <= 14; h++) {
				if (h == 0)
					strcpy(labels[h + 12], "UTC");
				else
					snprintf(labels[h + 12], sizeof(labels[0]), "UTC%+d", h);
				zones[h + 12] = labels[h + 12];
				if (h * 60 == minutes)
					current = h + 12;
			}
			zones[27] = NULL;
			h = osd_choose("Time Zone", zones, current);
			if (h >= 0 && h != current) {
				save_setting_int("timezone", (h - 12) * 60);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_REBOOT:
			power(0);
			return;
		default:
			return;
		}
	}
}

/* ---- Information ----------------------------------------------------------- */

static void about(void)
{
	char text[1800], status[64], speed[160];
	int w, h;
	host_display_size(&w, &h);
	host_network_status(status, sizeof(status));
	bmdos_status_text(speed, sizeof speed);
	snprintf(text, sizeof(text),
	         "BMDOS " BMDOS_VERSION "\n"
	         "Bare metal MS-DOS PC emulator for the Raspberry Pi.\n\n"
	         "Emulation: DOSBox Pure " DOSBOX_PURE_VERSION " (GPL 2 or later),\n"
	         "by Bernhard Schelling, from DOSBox by the DOSBox Team.\n"
	         "Based on BMNeXT, BMAmiga, BMCoco, BMA800XL and BMC64.\n\n"
	         "Running: %s\n"
	         "%s\n"
	         "Display: %dx%d %.2f Hz\n"
	         "Network: %s",
	         bmdos_content()[0] ? base_name(bmdos_content()) : "nothing (DOSBox Pure's menu)",
	         speed, w, h, bmdos_refresh_rate(), status);
	osd_info("About BMDOS", text);
}

static void offer_reboot(void)
{
	if (osd_confirm("Reboot?", "Settings changed that need a reboot. Reboot now?"))
		power(0);
}

/* ---- Main menu ---------------------------------------------------------------- */

void bmdos_menu(void)
{
	enum { M_RUN, M_EJECT, M_RESTART, M_COREMENU, M_DOSBOX, M_PADS, M_VIDEO, M_SOUND, M_NETWORK, M_ABOUT,
	       M_REBOOT, M_OFF };
	static int sel = 0;
	char title[48], running[OSD_COLS + 1];

	snprintf(title, sizeof(title), "BMDOS %s", BMDOS_VERSION);
	reboot_needed = FALSE;
	leave_menu = FALSE;
	osd_open();
	for (;;) {
		const char *now = bmdos_content();
		struct osd_item items[] = {
			{"Run a Game or Program", NULL, OSD_ITEM_SUBMENU, M_RUN},
			{"Run Nothing (DOSBox Pure's Menu)", NULL, 0, M_EJECT},
			{"Restart the PC", NULL, 0, M_RESTART},
			{"DOSBox Pure's Menu and Keyboard", NULL, 0, M_COREMENU},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"DOSBox Settings", NULL, OSD_ITEM_SUBMENU, M_DOSBOX},
			{"Gamepads", NULL, OSD_ITEM_SUBMENU, M_PADS},
			{"Video", NULL, OSD_ITEM_SUBMENU, M_VIDEO},
			{"Sound", NULL, OSD_ITEM_SUBMENU, M_SOUND},
			{"Network", NULL, OSD_ITEM_SUBMENU, M_NETWORK},
			{"About", NULL, OSD_ITEM_SUBMENU, M_ABOUT},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Reboot", NULL, 0, M_REBOOT},
			{"Shut Down", NULL, 0, M_OFF},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{running, NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen;
		snprintf(running, sizeof running, "Running: %s",
		         now[0] ? base_name(now) : "nothing");
		chosen = osd_menu(title, items, sizeof(items) / sizeof(items[0]), &sel);

		switch (chosen) {
		case M_RUN: choose_content(); break;
		case M_EJECT:
			bmdos_load_content("");
			leave_menu = TRUE;
			break;
		case M_RESTART:
			bmdos_restart();
			leave_menu = TRUE;
			break;
		case M_COREMENU:
			bmdos_open_core_menu();
			leave_menu = TRUE;
			break;
		case M_DOSBOX: dosbox_menu(); break;
		case M_PADS: gamepads_menu(); break;
		case M_VIDEO: video_menu(); break;
		case M_SOUND: sound_menu(); break;
		case M_NETWORK: network_menu(); break;
		case M_ABOUT: about(); break;
		case M_REBOOT:
			if (osd_confirm("Reboot", "Reboot BMDOS?"))
				power(0);
			break;
		case M_OFF:
			if (osd_confirm("Shut Down", "Shut down? It's then safe to switch off."))
				power(1);
			break;
		default:
			leave_menu = TRUE;
			break;
		}
		if (leave_menu || osd_closing() || bmdos_pending_requests())
			break;
	}
	if (reboot_needed && !osd_closing())
		offer_reboot();
	osd_close();
}
