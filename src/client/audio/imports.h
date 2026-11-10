/* Audio imports use the game import table and retain their calling convention. */
#ifndef CL_AUDIO_IMPORTS_H
#define CL_AUDIO_IMPORTS_H

void *sr_import(const char *dll, const char *name);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

/* Resolve each entry once. */
#define AUDIO_IMPORT(dll, convention, result, name, parameters) \
static __inline result (convention *audio_##name(void)) parameters \
{ \
    static result (convention *function) parameters; \
    if (!function) *(void **)&function = sr_import(dll, #name); \
    return function; \
}

AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void *, speex_lib_get_mode, (int))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void *, speex_encoder_init, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void *, speex_decoder_init, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, int, speex_encoder_ctl, (void *, int, void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void, speex_bits_init, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void, speex_bits_destroy, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void, speex_bits_reset, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void, speex_encoder_destroy, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, void, speex_decoder_destroy, (void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, int, speex_encode, (void *, float *, void *))
AUDIO_IMPORT("LIBSPEEX.DLL", __cdecl, int, speex_bits_write, (void *, char *, int))
AUDIO_IMPORT("LIBSPEEXDSP.DLL", __cdecl, void *, speex_preprocess_state_init, (int, int))
AUDIO_IMPORT("LIBSPEEXDSP.DLL", __cdecl, int, speex_preprocess_ctl, (void *, int, void *))
AUDIO_IMPORT("LIBSPEEXDSP.DLL", __cdecl, void, speex_preprocess_state_destroy, (void *))
AUDIO_IMPORT("LIBSPEEXDSP.DLL", __cdecl, int, speex_preprocess_run, (void *, short *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void *, alcOpenDevice, (char *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void *, alcCreateContext, (void *, int *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, char, alcMakeContextCurrent, (void *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, char *, alcGetString, (void *, int))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void *, alcCaptureOpenDevice, (char *, unsigned, int, int))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, char, alcCaptureCloseDevice, (void *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void *, alcGetCurrentContext, (void))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void *, alcGetContextsDevice, (void *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alcDestroyContext, (void *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, char, alcCloseDevice, (void *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alcGetIntegerv, (void *, int, int, int *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alcCaptureSamples, (void *, void *, int))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourceStop, (unsigned))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alListenerfv, (int, float *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alListenerf, (int, float))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourcefv, (unsigned, int, float *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alGetSourcei, (unsigned, int, int *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourceUnqueueBuffers, (unsigned, int, unsigned *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourceQueueBuffers, (unsigned, int, unsigned *))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSource3f, (unsigned, int, float, float, float))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourcei, (unsigned, int, int))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourcef, (unsigned, int, float))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alSourcePlay, (unsigned))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, void, alBufferData, (unsigned, int, void *, int, int))

#endif
