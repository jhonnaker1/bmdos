/*
 * coreopts.c - DOSBox Pure's settings, kept in bmdos.cfg.
 *
 * A libretro core describes its settings ("core options") to the frontend:
 * DOSBox Pure gives each one's key ("dosbox_pure_cycles"), its description,
 * a category (Emulation, Performance, Video, Audio, ...), its possible
 * values and the default (RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2), and then
 * asks for each one's value (RETRO_ENVIRONMENT_GET_VARIABLE), again when
 * told that one has changed (GET_VARIABLE_UPDATE). Its own descriptions
 * are what BMDOS's menu shows (menu.c, "DOSBox Settings"), so every
 * setting the core has can be changed, a new core's too, with nothing here
 * to add.
 *
 * The values are in bmdos.cfg on the SD card, as "dosbox_pure_cycles=max"
 * lines among BMDOS's own settings (host_option()); a setting not there
 * has its default: BMDOS's own where it differs from DOSBox Pure's
 * (bmdos_option_bmdos_default()), else the core's.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#include "../host.h"
#include "bmdos.h"

#define MAX_OPTIONS 160
#define VALUE_SIZE 64

static const struct retro_core_options_v2 *defs;
static int n_options, n_categories;
static char values[MAX_OPTIONS][VALUE_SIZE];
static unsigned char hidden[MAX_OPTIONS];
static int changed;

/* The host's sound output rate, for the core's (platform.c sets it). */
unsigned bmdos_sound_rate;

const char *bmdos_option_bmdos_default(const char *key)
{
	/* The Pi's HDMI plays at 48 kHz, the Pi 3's headphone jack at 44.1:
	   the core's sound at that rate needs no resampling. */
	if (strcmp(key, "dosbox_pure_audiorate") == 0)
		return bmdos_sound_rate == 44100 ? "44100" : "48000";
	/* The Voodoo's triangles drawn by the other cores too (BMDOS's own
	   threads take turns on one core, so its triangle workers are the
	   core pool's: src/patches/dosbox_pure_core_pool_patch.diff). Without
	   a GPU for OpenGL "auto" would be the same. */
	if (strcmp(key, "dosbox_pure_voodoo_perf") == 0)
		return "1";
	/* With automatic cycles DOSBox Pure fills this much of each frame.
	   Elsewhere its thread runs beside the frontend's; here it takes
	   turns with BMDOS's work on one core, so at 100% a frame took longer
	   than the display's refresh: Doom, Duke 3D and Tomb Raider ran at
	   50-80% on the Pi 5, and the sound ran dry (crackling). */
	if (strcmp(key, "dosbox_pure_cycle_limit") == 0)
		return "0.8";
	/* A USB mouse moves the DOS mouse (the core's default, a touch pad's
	   absolute position, is for touch screens). */
	if (strcmp(key, "dosbox_pure_mouse_input") == 0)
		return "true";
	return NULL;
}

static int find(const char *key)
{
	int i;
	if (key == NULL)
		return -1;
	for (i = 0; i < n_options; i++)
		if (strcmp(defs->definitions[i].key, key) == 0)
			return i;
	return -1;
}

static int has_value(int i, const char *value)
{
	const struct retro_core_option_v2_definition *d = &defs->definitions[i];
	int v;
	for (v = 0; v < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[v].value != NULL; v++)
		if (strcmp(d->values[v].value, value) == 0)
			return 1;
	return 0;
}

static void load_value(int i)
{
	const struct retro_core_option_v2_definition *d = &defs->definitions[i];
	const char *v = host_option(d->key, NULL);
	if (v == NULL || !has_value(i, v))
		v = bmdos_option_bmdos_default(d->key);
	if (v == NULL || !has_value(i, v))
		v = d->default_value;
	if (v == NULL)
		v = d->values[0].value != NULL ? d->values[0].value : "";
	snprintf(values[i], VALUE_SIZE, "%s", v);
}

void bmdos_options_define(const struct retro_core_options_v2 *options)
{
	int i;
	defs = options;
	n_options = n_categories = 0;
	if (options == NULL)
		return;
	while (n_options < MAX_OPTIONS && options->definitions[n_options].key != NULL)
		n_options++;
	if (options->categories != NULL)
		while (options->categories[n_categories].key != NULL)
			n_categories++;
	for (i = 0; i < n_options; i++) {
		load_value(i);
		hidden[i] = 0;
	}
	LOG("%d DOSBox Pure settings in %d categories\n", n_options, n_categories);
}

int bmdos_options_count(void) { return n_options; }
int bmdos_categories_count(void) { return n_categories; }

const char *bmdos_option_key(int i)
{
	return i >= 0 && i < n_options ? defs->definitions[i].key : NULL;
}

/* The description within its category ("Show Statistics", not
   "Advanced > Show Statistics") where the core has one. */
const char *bmdos_option_desc(int i)
{
	const struct retro_core_option_v2_definition *d;
	if (i < 0 || i >= n_options)
		return NULL;
	d = &defs->definitions[i];
	return d->desc_categorized != NULL ? d->desc_categorized : d->desc;
}

const char *bmdos_option_info(int i)
{
	const struct retro_core_option_v2_definition *d;
	if (i < 0 || i >= n_options)
		return NULL;
	d = &defs->definitions[i];
	return d->info_categorized != NULL ? d->info_categorized : d->info;
}

const char *bmdos_option_category(int i)
{
	if (i < 0 || i >= n_options || defs->definitions[i].category_key == NULL)
		return "";
	return defs->definitions[i].category_key;
}

int bmdos_option_values(int i, const char **vals, const char **labels, int n)
{
	const struct retro_core_option_v2_definition *d;
	int v;
	if (i < 0 || i >= n_options)
		return 0;
	d = &defs->definitions[i];
	for (v = 0; v < n && v < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[v].value != NULL; v++) {
		vals[v] = d->values[v].value;
		labels[v] = d->values[v].label != NULL ? d->values[v].label : d->values[v].value;
	}
	return v;
}

const char *bmdos_category_key(int i)
{
	return i >= 0 && i < n_categories ? defs->categories[i].key : NULL;
}

const char *bmdos_category_desc(int i)
{
	return i >= 0 && i < n_categories ? defs->categories[i].desc : NULL;
}

const char *bmdos_option_get(const char *key)
{
	int i = find(key);
	return i >= 0 ? values[i] : NULL;
}

int bmdos_option_set(const char *key, const char *value)
{
	int i = find(key);
	if (i < 0 || value == NULL)
		return 0;
	if (strcmp(values[i], value) != 0) {
		snprintf(values[i], VALUE_SIZE, "%s", value);
		changed = TRUE;
	}
	host_option_set(key, value);
	host_options_save();
	return 1;
}

int bmdos_option_visible(int i)
{
	return i >= 0 && i < n_options && !hidden[i];
}

void bmdos_option_set_visible(const char *key, int visible)
{
	int i = find(key);
	if (i >= 0)
		hidden[i] = !visible;
}

int bmdos_options_changed(void)
{
	int c = changed;
	changed = FALSE;
	return c;
}
