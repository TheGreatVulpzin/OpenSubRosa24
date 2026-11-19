/* The sound tests of the client. See test_voice.c. */
#ifndef CL_AUDIO_TEST_VOICE_H
#define CL_AUDIO_TEST_VOICE_H

#include "sr_types.h"

void __cdecl testspeex(int sound, const char *file, int mode);
void __cdecl testvoice(int sound, int player);

#endif /* CL_AUDIO_TEST_VOICE_H */
