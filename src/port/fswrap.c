/*
 * fswrap.c - file system fixes for Circle's FatFs.
 *
 * FatFs cannot stat() the root directory of a volume, so the file
 * browser would think "/" doesn't exist and refuse to go up to it. The
 * kernel is linked with --wrap=stat, so every call to stat() comes here
 * first; the root (and ".") are reported as directories, everything else
 * goes to the real stat().
 *
 * readdir() (--wrap=readdir) gives a dirent with d_type after newlib's
 * (src/port/circle/include/sys/dirent.h, which DOSBox Pure is built with:
 * it reads d_type), set to DT_UNKNOWN, so that DOSBox asks stat() whether
 * an entry is a folder. It also leaves out the Mac's files: "._" ones
 * (macOS keeps a file's extended attributes in them on FAT32) and
 * .DS_Store (from BMAmiga).
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator
 * for the Raspberry Pi. See LICENSE for licensing details.
 */

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

int __real_stat(const char *path, struct stat *st);

static int is_root(const char *path)
{
	const char *colon = strchr(path, ':');
	/* Skip a volume name such as "SD:" */
	if (colon != NULL)
		path = colon + 1;
	while (*path == '/')
		path++;
	return *path == '\0' || strcmp(path, ".") == 0 || strcmp(path, "./") == 0;
}

int __wrap_stat(const char *path, struct stat *st)
{
	if (path != NULL && st != NULL && is_root(path)) {
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		st->st_nlink = 1;
		return 0;
	}
	return __real_stat(path, st);
}

struct dirent *__real_readdir(DIR *dir);

/* newlib's entry with d_type after it (sys/dirent.h in src/port/circle). */
static struct {
	struct dirent entry;
	unsigned char d_type;
} with_type;

static int is_mac_file(const char *name)
{
	return (name[0] == '.' && name[1] == '_' && name[2] != '\0') || strcmp(name, ".DS_Store") == 0;
}

struct dirent *__wrap_readdir(DIR *dir)
{
	struct dirent *entry;
	while ((entry = __real_readdir(dir)) != NULL && is_mac_file(entry->d_name))
		;
	if (entry == NULL)
		return NULL;
	with_type.entry = *entry;
	with_type.d_type = 0;   /* DT_UNKNOWN */
	return &with_type.entry;
}
