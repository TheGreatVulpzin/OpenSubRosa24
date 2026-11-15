/* The OpenAL buffers and the start of a sound. See sound_create.c. */
#ifndef CL_AUDIO_SOUND_CREATE_H
#define CL_AUDIO_SOUND_CREATE_H

#include "sr_types.h"

void __cdecl setupsounds(void);
int __cdecl createsoundobject(int buffer, const vec3 *position, const vec3 *velocity, float gain, float pitch,
                              int keep_alive, int object, int kind);
int __cdecl createsound(int buffer, const vec3 *position, const vec3 *velocity, float gain, float pitch);

#endif /* CL_AUDIO_SOUND_CREATE_H */
