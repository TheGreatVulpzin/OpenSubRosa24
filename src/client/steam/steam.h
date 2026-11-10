/* The Steam interface of the client. See steam.c. */
#ifndef CL_STEAM_STEAM_H
#define CL_STEAM_STEAM_H

#include <stdint.h>

int __cdecl steam_init(void);
void __cdecl steam_shutdown(void);
void __cdecl steam_update(void);
int __cdecl steam_getname(char *name);
void __cdecl steam_getid(uint64_t *id);

#endif /* CL_STEAM_STEAM_H */
