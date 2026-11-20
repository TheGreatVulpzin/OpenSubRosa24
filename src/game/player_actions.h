/* Player action rings and road waypoints. */
#ifndef SR_PLAYER_ACTIONS_H
#define SR_PLAYER_ACTIONS_H

void player_addaction_item(int player, int first, int second);
void player_addaction_chat(int player, const char *text);
void addplayerwaypoint(int player, float x, float y, float z);

#endif
