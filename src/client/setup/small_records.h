/* Small functions on the records of the client. See small_records.c. */
#ifndef CL_SETUP_SMALL_RECORDS_H
#define CL_SETUP_SMALL_RECORDS_H

#include "sr_types.h"

int __cdecl getnextplayer(void);
int __cdecl getnextobject(void);
int __cdecl getnextitem(void);
int __cdecl getnexthuman(void);
void __cdecl copyquaternion(uint32_t *to, const uint32_t *from);
void __cdecl addpointstoteam(int team, int points);
void __cdecl accountdeathtax(int index);
float __cdecl gettextlength(const char *text, float scale);
float __cdecl fround(float value, float step);
long long __cdecl net_calcdistancefactor(float distance);
float __cdecl raydistancesphere2(const vec3 *origin, const vec3 *direction, float radius);

#endif /* CL_SETUP_SMALL_RECORDS_H */
