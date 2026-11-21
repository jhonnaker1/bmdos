/*
 * vfs.c - the libretro file system interface, for DOSBox Pure.
 *
 * DOSBox Pure reads its content (a ZIP, a disk image, a program) and
 * writes its saves with the C library (fopen() and the rest), which on the
 * Pi is circle-newlib over FatFs: the SD card ("SD:/") and the USB sticks
 * ("USB:/", "USB2:/", "USB3:/") alike. For the rest it asks the frontend for
 * the libretro VFS (RETRO_ENVIRONMENT_GET_VFS_INTERFACE): it lists the
 * system folder with it (operating system images and the shell's ZIPs in
 * SD:/dos/system), and checks with it that a file exists. This is that
 * interface, version 3, over the same C library, so both ways see the same
 * files.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libretro.h"

#include "bmdos.h"

struct retro_vfs_file_handle {
	FILE *f;
	char *path;
};

struct retro_vfs_dir_handle {
	DIR *d;
	char *path;
	struct dirent *entry;
	int is_dir;
	int include_hidden;
};

static const char *vfs_get_path(struct retro_vfs_file_handle *stream)
{
	return stream != NULL ? stream->path : NULL;
}

static struct retro_vfs_file_handle *vfs_open(const char *path, unsigned mode, unsigned hints)
{
	struct retro_vfs_file_handle *h;
	const char *m;
	FILE *f;
	(void) hints;
	if (path == NULL)
		return NULL;
	if (mode == RETRO_VFS_FILE_ACCESS_READ)
		m = "rb";
	else if (mode == RETRO_VFS_FILE_ACCESS_WRITE)
		m = "wb";
	else if (mode == RETRO_VFS_FILE_ACCESS_READ_WRITE)
		m = "w+b";
	else if (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) {
		/* Read and write (or write) without discarding: "r+b", or a new
		   file where there is none. */
		m = "r+b";
		f = fopen(path, m);
		if (f == NULL && errno == ENOENT)
			f = fopen(path, "w+b");
		goto opened;
	} else
		return NULL;
	f = fopen(path, m);
opened:
	if (f == NULL)
		return NULL;
	h = calloc(1, sizeof *h);
	if (h == NULL || (h->path = strdup(path)) == NULL) {
		free(h);
		fclose(f);
		return NULL;
	}
	h->f = f;
	return h;
}

static int vfs_close(struct retro_vfs_file_handle *stream)
{
	int r;
	if (stream == NULL)
		return -1;
	r = fclose(stream->f);
	free(stream->path);
	free(stream);
	return r == 0 ? 0 : -1;
}

static int64_t vfs_tell(struct retro_vfs_file_handle *stream)
{
	return stream != NULL ? (int64_t) ftello(stream->f) : -1;
}

static int64_t vfs_seek(struct retro_vfs_file_handle *stream, int64_t offset, int seek_position)
{
	int whence = seek_position == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR
	           : seek_position == RETRO_VFS_SEEK_POSITION_END ? SEEK_END : SEEK_SET;
	if (stream == NULL || fseeko(stream->f, (off_t) offset, whence) != 0)
		return -1;
	return (int64_t) ftello(stream->f);
}

static int64_t vfs_size(struct retro_vfs_file_handle *stream)
{
	off_t here, end;
	if (stream == NULL)
		return -1;
	here = ftello(stream->f);
	if (fseeko(stream->f, 0, SEEK_END) != 0)
		return -1;
	end = ftello(stream->f);
	fseeko(stream->f, here, SEEK_SET);
	return (int64_t) end;
}

static int64_t vfs_read(struct retro_vfs_file_handle *stream, void *s, uint64_t len)
{
	if (stream == NULL)
		return -1;
	return (int64_t) fread(s, 1, (size_t) len, stream->f);
}

static int64_t vfs_write(struct retro_vfs_file_handle *stream, const void *s, uint64_t len)
{
	if (stream == NULL)
		return -1;
	return (int64_t) fwrite(s, 1, (size_t) len, stream->f);
}

static int vfs_flush(struct retro_vfs_file_handle *stream)
{
	return stream != NULL && fflush(stream->f) == 0 ? 0 : -1;
}

static int vfs_remove(const char *path)
{
	return path != NULL && remove(path) == 0 ? 0 : -1;
}

static int vfs_rename(const char *old_path, const char *new_path)
{
	return old_path != NULL && new_path != NULL && rename(old_path, new_path) == 0 ? 0 : -1;
}

static int64_t vfs_truncate(struct retro_vfs_file_handle *stream, int64_t length)
{
	if (stream == NULL || fflush(stream->f) != 0)
		return -1;
	return ftruncate(fileno(stream->f), (off_t) length) == 0 ? 0 : -1;
}

static int vfs_stat(const char *path, int32_t *size)
{
	struct stat st;
	int flags;
	if (path == NULL || stat(path, &st) != 0)
		return 0;
	flags = RETRO_VFS_STAT_IS_VALID;
	if (S_ISDIR(st.st_mode))
		flags |= RETRO_VFS_STAT_IS_DIRECTORY;
	if (size != NULL)
		*size = (int32_t) st.st_size;
	return flags;
}

/* 0 made, -2 there already, -1 failed (libretro's convention). */
static int vfs_mkdir(const char *dir)
{
	struct stat st;
	if (dir == NULL)
		return -1;
	if (mkdir(dir, 0777) == 0)
		return 0;
	if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode))
		return -2;
	return -1;
}

static struct retro_vfs_dir_handle *vfs_opendir(const char *dir, bool include_hidden)
{
	struct retro_vfs_dir_handle *h;
	DIR *d;
	if (dir == NULL || (d = opendir(dir[0] != '\0' ? dir : ".")) == NULL)
		return NULL;
	h = calloc(1, sizeof *h);
	if (h == NULL || (h->path = strdup(dir)) == NULL) {
		free(h);
		closedir(d);
		return NULL;
	}
	h->d = d;
	h->include_hidden = include_hidden;
	return h;
}

static bool vfs_readdir(struct retro_vfs_dir_handle *dirstream)
{
	if (dirstream == NULL)
		return false;
	for (;;) {
		const char *name;
		dirstream->entry = readdir(dirstream->d);
		if (dirstream->entry == NULL)
			return false;
		name = dirstream->entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
			continue;
		/* A Mac's "._" files (resource forks it leaves on the card) are
		   never anything DOS wants. */
		if (name[0] == '.' && (!dirstream->include_hidden || name[1] == '_'))
			continue;
		break;
	}
	{
		/* Whether it is a folder: newlib's dirent has no d_type, so stat. */
		char full[1024];
		struct stat st;
		size_t n = strlen(dirstream->path);
		snprintf(full, sizeof full, "%s%s%s", dirstream->path,
		         n > 0 && dirstream->path[n - 1] != '/' ? "/" : "", dirstream->entry->d_name);
		dirstream->is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
	}
	return true;
}

static const char *vfs_dirent_get_name(struct retro_vfs_dir_handle *dirstream)
{
	return dirstream != NULL && dirstream->entry != NULL ? dirstream->entry->d_name : NULL;
}

static bool vfs_dirent_is_dir(struct retro_vfs_dir_handle *dirstream)
{
	return dirstream != NULL && dirstream->is_dir;
}

static int vfs_closedir(struct retro_vfs_dir_handle *dirstream)
{
	if (dirstream == NULL)
		return -1;
	closedir(dirstream->d);
	free(dirstream->path);
	free(dirstream);
	return 0;
}

static const struct retro_vfs_interface vfs = {
	vfs_get_path, vfs_open, vfs_close, vfs_size, vfs_tell, vfs_seek, vfs_read,
	vfs_write, vfs_flush, vfs_remove, vfs_rename, vfs_truncate,
	vfs_stat, vfs_mkdir, vfs_opendir, vfs_readdir, vfs_dirent_get_name,
	vfs_dirent_is_dir, vfs_closedir,
};

const struct retro_vfs_interface *bmdos_vfs(void)
{
	return &vfs;
}
