/*
 * sys/dirent.h for DOSBox Pure on Circle: circle-stdlib's, without FatFs's
 * header (wrap_fatfs.h), whose Windows style types (DWORD, BYTE...) clash
 * with a core's. It was only needed for FF_LFN_BUF, the length of a
 * name, which is 255 in Circle's FatFs (addon/fatfs/ffconf.h): struct
 * dirent starts as circle-stdlib's does.
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define BMAMIGA_FF_LFN_BUF 255

typedef struct _CIRCLE_DIR DIR;

/* newlib's, with d_type after it: DOSBox (src/misc/cross.cpp) reads it.
   readdir() is wrapped (src/port/fswrap.c) to copy newlib's entry into
   this one with d_type DT_UNKNOWN, so that DOSBox asks stat(). */
struct dirent {
	ino_t d_ino;
	char d_name[BMAMIGA_FF_LFN_BUF + 1];
	unsigned char d_type;
};

#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int readdir_r(DIR *__restrict, struct dirent *__restrict, struct dirent **__restrict);
void rewinddir(DIR *);
int closedir(DIR *);

#ifdef __cplusplus
}
#endif

#endif
