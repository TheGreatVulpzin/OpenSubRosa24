/* The tickets of the client. See tickets.c. */
#ifndef CL_SETUP_TICKETS_H
#define CL_SETUP_TICKETS_H

#include "sr_types.h"

void __cdecl ticketreturned(const void *ticket, uint8_t size);
const void *__cdecl steam_getauthticket(uint32_t *size);
void __cdecl initplayticket(void);

#endif /* CL_SETUP_TICKETS_H */
