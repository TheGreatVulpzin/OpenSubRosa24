/* Spatial sound sources and voice playback. */
#ifndef CL_AUDIO_SOUND_H
#define CL_AUDIO_SOUND_H
#include "sr_types.h"
void soundsimulation(const vec3 *position, const vec3 rotation[3], const vec3 *velocity);
void deletesound(int index);
#endif
