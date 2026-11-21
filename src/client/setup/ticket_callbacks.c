/* Application ticket callbacks.
 * The pending call has two words, a receiver and a member method.
 * A response clears the call before it invokes the member method.
 */
#include "setup/ticket_callbacks.h"

#include "client.h"
#include "sr_runtime.h"

typedef void (__fastcall *TicketMethod)(void *receiver, int unused_edx, void *response, unsigned char failed);

/* Game functions of other files. */
#define construct_callresult CL_FN(0x50c040, AppTicket *(__fastcall *)(AppTicket *))
#define destroy_callresult CL_FN(0x50c190, void(__fastcall *)(AppTicket *))

/* Initialize the callback base and clear the ticket callback. */
AppTicket *__fastcall appticket_construct(AppTicket *ticket)
{
    construct_callresult(ticket);
    ticket->ticket_callback = 0;
    return ticket;
}

/* Deliver a response with no I/O failure. */
void __fastcall callresult_run(AppTicket *ticket, int unused_edx, void *response)
{
    (void)unused_edx;
    ticket->call_low = 0;
    ticket->call_high = 0;
    ((TicketMethod)ticket->method)(ticket->receiver, 0, response, 0);
}

/* Deliver a response only when both words identify the pending call. */
void __fastcall callresult_runhandle(AppTicket *ticket, int unused_edx, void *response,
                                    unsigned char failed, uint32_t low, uint32_t high)
{
    (void)unused_edx;
    if (low == ticket->call_low && high == ticket->call_high) {
        ticket->call_low = 0;
        ticket->call_high = 0;
        ((TicketMethod)ticket->method)(ticket->receiver, 0, response, failed);
    }
}

/* Return the four-byte size of an encrypted ticket response. */
int __fastcall callresult_getsize(AppTicket *ticket)
{
    enum { ENCRYPTED_TICKET_RESPONSE_SIZE = 4 };
    (void)ticket;
    return ENCRYPTED_TICKET_RESPONSE_SIZE;
}

/* Cancel the pending call and release the callback base. */
void __fastcall appticket_destroy(AppTicket *ticket)
{
    destroy_callresult(ticket);
}

/* Release the application ticket during process exit. */
void __cdecl appticket_cleanup(void)
{
    appticket_destroy(&cl_data_app_ticket);
}
