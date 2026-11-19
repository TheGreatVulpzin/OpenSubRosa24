/* The chat messages of the screen. See chat_message.c. */
#ifndef CL_SETUP_CHAT_MESSAGE_H
#define CL_SETUP_CHAT_MESSAGE_H

#include "sr_types.h"

void __cdecl createmessage(int kind, const char *text, int target, int style);

#endif /* CL_SETUP_CHAT_MESSAGE_H */
