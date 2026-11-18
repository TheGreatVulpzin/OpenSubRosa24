/* Timed messages for the display.
 * New messages replace entries in a ring of 256 records.
 * The caller supplies text that fits the message buffer.
 */
#include "setup/messages.h"
#include <string.h>
#include "sr_runtime.h"

/* Store a message and select its display time from its kind. */
void createmessage(int kind, const char *text, int target, int style)
{
    ChatMessage *message = &cl_data_chatmessage[cl_data_chat_head];
    message->kind = kind;
    strcpy(message->text, text);
    message->delay = (int)strlen(text);
    message->timer = kind == 0 || kind == 4 ? 600 : 480;
    message->target = target;
    message->style = style;
    cl_data_chat_head = (cl_data_chat_head + 1) & 255;
}
