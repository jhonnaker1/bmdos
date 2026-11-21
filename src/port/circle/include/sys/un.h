/*
 * sys/un.h for Amiberry on Circle (no local sockets: the address only).
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMDOS_CIRCLE_SYS_UN_H_
#define BMDOS_CIRCLE_SYS_UN_H_

#include <sys/socket.h>

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

#endif /* BMDOS_CIRCLE_SYS_UN_H_ */
