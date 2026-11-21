/*
 * netinet/in.h for Amiberry on Circle: circle-stdlib's, with the byte
 * order functions it declares only in arpa/inet.h.
 *
 * From BMAmiga. This file is part of BMDOS, a bare metal MS-DOS PC
 * emulator for the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMDOS_CIRCLE_NETINET_IN_H_
#define BMDOS_CIRCLE_NETINET_IN_H_

#include <stdint.h>
#include_next <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);
#ifdef __cplusplus
}
#endif

#endif /* BMDOS_CIRCLE_NETINET_IN_H_ */
