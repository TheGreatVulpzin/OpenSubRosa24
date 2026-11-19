/* The ring of actions that a player sends to the server: a chat line and an item action. */
#include "menu/player_actions.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define ACTION_MASK 0x3f
#define ACTION_CHAT 1
#define ACTION_ITEM 2
#define CHAT_LENGTH_MASK 0x3f

/* Add a chat line to the ring of actions of a player. The line is cut to 63 characters and has no
 * end mark. The action keeps the voice mode of the client. */
void __cdecl player_addaction_chat(int player, const char *text)
{
    Player *p = &sr_data_player[player];
    PlayerAction *action = &p->actions[p->num_actions];
    size_t length = strlen(text);

    action->kind = ACTION_CHAT;
    action->arg_a = (int)(length & CHAT_LENGTH_MASK);
    memcpy(action->text, text, length & CHAT_LENGTH_MASK);
    action->arg_b = cl_data_audio_voice_mode;
    p->num_actions = (p->num_actions + 1) & ACTION_MASK;
}

/* Add an item action to the ring of actions of a player: two item numbers. */
void __cdecl player_addaction_item(int player, int item_a, int item_b)
{
    Player *p = &sr_data_player[player];
    PlayerAction *action = &p->actions[p->num_actions];

    action->kind = ACTION_ITEM;
    action->arg_a = item_a;
    action->arg_b = item_b;
    p->num_actions = (p->num_actions + 1) & ACTION_MASK;
}
