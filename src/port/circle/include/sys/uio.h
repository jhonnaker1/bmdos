/*
 * sys/uio.h for Amiberry on Circle.
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMDOS_CIRCLE_SYS_UIO_H_
#define BMDOS_CIRCLE_SYS_UIO_H_

#include <sys/types.h>

struct iovec {
	void *iov_base;
	size_t iov_len;
};

#ifdef __cplusplus
extern "C" {
#endif

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

#ifdef __cplusplus
}
#endif

#endif /* BMDOS_CIRCLE_SYS_UIO_H_ */
