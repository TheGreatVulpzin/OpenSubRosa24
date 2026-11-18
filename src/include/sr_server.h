/* Shared game services and server entry points. */
#ifndef SR_SERVER_H
#define SR_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* accounts.c */
void __cdecl sr_loadaccountsserver(void);

/* src/net/message_events.c */
void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra);
void __cdecl sr_createevent_updatedoor(int a, int b);
void __cdecl sr_createevent_updatemission(int team, int mission);

/* src/game/player_names.c */
void __cdecl sr_fixplayername(int id);

/* main_loop.c (subvulpes.ini own_main=1) */
void __cdecl sr_getpaths(void);
void __cdecl sr_dedicatedserverloop(void);
int sv_main_loop_install(void);

#ifdef __cplusplus
}
#endif

#endif /* SR_SERVER_H */
