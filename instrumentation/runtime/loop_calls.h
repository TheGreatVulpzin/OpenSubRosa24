/* Enter through the installed hooks when the hook build owns the main loop. */
#ifndef SR_LOOP_CALLS_H
#define SR_LOOP_CALLS_H

#include <stdint.h>
extern uintptr_t sr_base;

#define getmasterserverip ((int (__cdecl *)(void))(sr_base + 0x1ac40))
#define systemtimer ((uint32_t (__cdecl *)(void))(sr_base + 0x5350))
#define disablesocket ((void (__cdecl *)(int))(sr_base + 0xd710))
#define init_genrand ((void (__cdecl *)(uint32_t))(sr_base + 0xad30))
#define playerai ((void (__cdecl *)(int))(sr_base + 0x4f720))
#define stoptimer ((void (__cdecl *)(int))(sr_base + 0xb3f0))
#define loadsaveoptions ((void (__cdecl *)(int))(sr_base + 0x4e7a0))
#define sr_loadaccountsserver ((void (__cdecl *)(void))(sr_base + 0xe2e0))
#define leveleditor_load ((void (__cdecl *)(void))(sr_base + 0x8590))
#define setupgame ((void (__cdecl *)(void))(sr_base + 0x502a0))
#define resetgame ((void (__cdecl *)(void))(sr_base + 0x590f0))
#define servercycle ((void (__cdecl *)(void))(sr_base + 0x33570))
#define serverrecv ((void (__cdecl *)(void))(sr_base + 0x33650))
#define playersimulation ((void (__cdecl *)(void))(sr_base + 0xa2a0))
#define particlesimulation ((void (__cdecl *)(void))(sr_base + 0x27ba0))
#define objectsimulation ((void (__cdecl *)(void))(sr_base + 0x56710))
#define humansimulation ((void (__cdecl *)(void))(sr_base + 0x51060))
#define itemsimulation ((void (__cdecl *)(void))(sr_base + 0x542a0))
#define bondsimulation2 ((void (__cdecl *)(void))(sr_base + 0x41de0))
#define itemlogic ((void (__cdecl *)(void))(sr_base + 0x4c8c0))
#define itemtimetolive ((void (__cdecl *)(void))(sr_base + 0x30ab0))
#define objecttimetolive ((void (__cdecl *)(void))(sr_base + 0x32340))
#define particletimetolive ((void (__cdecl *)(void))(sr_base + 0x279b0))
#define bondtimetolive ((void (__cdecl *)(void))(sr_base + 0x12c90))
#define logicsimulation ((void (__cdecl *)(void))(sr_base + 0x5c580))
#define serversend ((void (__cdecl *)(void))(sr_base + 0x4ed30))
#define bulletsimulation ((void (__cdecl *)(void))(sr_base + 0x48ec0))
#define eventsimulation ((void (__cdecl *)(void))(sr_base + 0x61a0))
#define bullettimetolive ((void (__cdecl *)(void))(sr_base + 0x14590))
#define debug_resetlogfile ((void (__cdecl *)(void))(sr_base + 0x5280))
#define getcpufrequency ((void (__cdecl *)(void))(sr_base + 0xb4a0))
#define setuptimers ((void (__cdecl *)(void))(sr_base + 0xb540))

#endif
