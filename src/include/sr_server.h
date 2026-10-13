/* Server functions rewritten in C.
 * Each function has the same signature and calling convention as the original in the 024d.
 */
#ifndef SR_SERVER_H
#define SR_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* events.c */
void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra);
void __cdecl sr_createevent_updatedoor(int a, int b);
void __cdecl sr_createevent_updatemission(int team, int mission);

/* players.c */
void __cdecl sr_fixplayername(int id);

/* main_loop.c (subvulpes.ini own_main=1) */
void __cdecl sr_getpaths(void);
void __cdecl sr_dedicatedserverloop(void);
int sv_main_loop_install(void);

#ifdef __cplusplus
}
#endif

#endif /* SR_SERVER_H */
