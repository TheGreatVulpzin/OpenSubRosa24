/* Queues for player actions and road waypoints.
 * Actions occupy a ring of sixty-four entries.
 * A waypoint appends its position and leaves its road fields unchanged.
 */
#include "player_actions.h"

#include <string.h>
#include "sr_runtime.h"

#ifndef sr_chat_mode
#define sr_chat_mode 0
#endif

enum { ACTION_CHAT = 1, ACTION_ITEM = 2, ACTION_MASK = 63 };

/* Queue an item action. The ring wraps after the last entry. */
void player_addaction_item(int player, int first, int second)
{
    Player *p = &sr_players[player];
    PlayerAction *action = &p->actions[p->num_actions];
    action->kind = ACTION_ITEM;
    action->arg_a = first;
    action->arg_b = second;
    p->num_actions = (p->num_actions + 1) & ACTION_MASK;
}

/* Queue the low six bits of the text length. The copied text has no terminator. */
void player_addaction_chat(int player, const char *text)
{
    Player *p = &sr_players[player];
    PlayerAction *action = &p->actions[p->num_actions];
    size_t length = strlen(text) & ACTION_MASK;
    action->kind = ACTION_CHAT;
    action->arg_a = (int)length;
    memcpy(action->text, text, length);
    action->arg_b = sr_chat_mode;
    p->num_actions = (p->num_actions + 1) & ACTION_MASK;
}

/* Append one position. The caller reserves a waypoint slot. */
void addplayerwaypoint(int player, float x, float y, float z)
{
    Player *p = &sr_players[player];
    AiWaypoint *waypoint = &p->waypoints[p->num_path];
    waypoint->pos.x = x;
    waypoint->pos.y = y;
    waypoint->pos.z = z;
    ++p->num_path;
}
