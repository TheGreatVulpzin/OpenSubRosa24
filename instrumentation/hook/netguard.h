/* Input checks on received datagrams (instrumentation/hook/netguard.c). */
#ifndef SV_NETGUARD_H
#define SV_NETGUARD_H

#include <stdint.h>

#define SV_GUARD_PASS 0  /* give the datagram to the game (maybe with masked fields) */
#define SV_GUARD_DROP 1  /* a type 4 packet that ends before its last voice frame */

/* Checks a datagram of len bytes in place. *masked gets the number of voice indexes that
 * were above 63 and are now masked to 6 bits. Datagrams other than type 4 pass unchanged. */
int sv_netguard_input(uint8_t *buf, int len, int *masked);

#endif
