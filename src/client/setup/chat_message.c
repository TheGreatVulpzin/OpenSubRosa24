/* The chat messages of the screen: a ring of 256 messages. */
#include "setup/chat_message.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define MESSAGE_RING_MASK 0xff
#define MESSAGE_KIND_SYSTEM 0
#define MESSAGE_KIND_NOTICE 4
#define MESSAGE_TICKS_LONG 600
#define MESSAGE_TICKS_SHORT 0x1e0

/* Add a message to the ring. The text has no length limit: a long text writes past the message. The
 * message stays 600 ticks for kind 0 and kind 4 and 480 ticks for every other kind. */
void __cdecl createmessage(int kind, const char *text, int target, int style)
{
    ChatMessage *message = &cl_data_chatmessage[cl_data_chat_head];

    message->kind = kind;
    strcpy(message->text, text);
    message->delay = (int)strlen(text);
    message->timer = (kind == MESSAGE_KIND_SYSTEM || kind == MESSAGE_KIND_NOTICE) ? MESSAGE_TICKS_LONG
                                                                                  : MESSAGE_TICKS_SHORT;
    message->target = target;
    message->style = style;
    cl_data_chat_head = (cl_data_chat_head + 1) & MESSAGE_RING_MASK;
}
