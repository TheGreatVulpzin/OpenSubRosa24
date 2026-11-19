/* The authentication ticket that the client holds, and the start of the play ticket. */
#include "setup/tickets.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define TICKET_LIMIT 0x400

/* Keep a ticket. A ticket of 1025 bytes or more is ignored. */
void __cdecl ticketreturned(const void *ticket, uint8_t size)
{
    if (size <= TICKET_LIMIT) {
        cl_data_auth_ticket_size = size;
        memcpy(cl_data_auth_ticket, ticket, size);
        cl_data_auth_ticket_ready = 1;
    }
}

/* Give the kept ticket and its size. Return null when no ticket is kept. */
const void *__cdecl steam_getauthticket(uint32_t *size)
{
    if (cl_data_auth_ticket_ready != 1)
        return NULL;
    *size = cl_data_auth_ticket_size;
    return cl_data_auth_ticket;
}

/* Set the play ticket to its start values: the third value is -1. */
void __cdecl initplayticket(void)
{
    cl_data_play_ticket[0] = 0;
    cl_data_play_ticket[1] = 0;
    cl_data_play_ticket[2] = -1;
    cl_data_play_ticket[3] = 0;
    cl_data_play_ticket_on = 1;
}
