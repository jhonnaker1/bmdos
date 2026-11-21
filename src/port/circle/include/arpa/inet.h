/*
 * arpa/inet.h for Amiberry on Circle: circle-stdlib's, which uses
 * socklen_t without defining it, and inet_aton().
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMDOS_CIRCLE_ARPA_INET_H_
#define BMDOS_CIRCLE_ARPA_INET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include_next <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif
int inet_aton(const char *cp, struct in_addr *inp);
#ifdef __cplusplus
}
#endif

#endif /* BMDOS_CIRCLE_ARPA_INET_H_ */
