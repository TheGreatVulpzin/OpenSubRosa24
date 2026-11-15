/* Load a sound file into an OpenAL buffer. See wav.c. */
#ifndef CL_AUDIO_WAV_H
#define CL_AUDIO_WAV_H

void __cdecl loadwav(int sound, const char *path, float reference);

#endif /* CL_AUDIO_WAV_H */
