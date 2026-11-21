/* The exit hook of the application ticket.
 * The client has no Steam, so no ticket call is ever pending.
 */
#include "setup/ticket_callbacks.h"

/* Nothing to release: the client never asks Steam for a ticket. */
void __cdecl appticket_cleanup(void)
{
}
