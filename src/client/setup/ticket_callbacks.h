/* Application ticket callbacks. */
#ifndef CL_SETUP_TICKET_CALLBACKS_H
#define CL_SETUP_TICKET_CALLBACKS_H

#include "sr_types.h"

AppTicket *__fastcall appticket_construct(AppTicket *ticket);
void __fastcall callresult_run(AppTicket *ticket, int unused_edx, void *response);
void __fastcall callresult_runhandle(AppTicket *ticket, int unused_edx, void *response,
                                    unsigned char failed, uint32_t low, uint32_t high);
int __fastcall callresult_getsize(AppTicket *ticket);
void __fastcall appticket_destroy(AppTicket *ticket);
void __cdecl appticket_cleanup(void);

#endif
