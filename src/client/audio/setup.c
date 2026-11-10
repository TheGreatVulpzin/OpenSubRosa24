/* Playback uses the default OpenAL device and its current context.
 * Capture uses mono signed samples at 8000 Hz.
 * Playback becomes enabled only after device and context setup succeeds.
 */
#include "audio/setup.h"
#include "audio/imports.h"
#include "sr_runtime.h"

enum { AUDIO_RATE = 8000, AUDIO_MONO16 = 0x1101, ALC_CAPTURE_DEFAULT_DEVICE = 0x311 };

/* Open playback and capture. Stop if the playback device or context is missing. */
void setupaudio(void)
{
    char *name;
    cl_data_audio_device = audio_alcOpenDevice()(0);
    if (!cl_data_audio_device) return;
    cl_data_audio_context = audio_alcCreateContext()(cl_data_audio_device, 0);
    if (!cl_data_audio_context) return;
    audio_alcMakeContextCurrent()(cl_data_audio_context);
    name = audio_alcGetString()(0, ALC_CAPTURE_DEFAULT_DEVICE);
    cl_data_audio_capture = audio_alcCaptureOpenDevice()(name, AUDIO_RATE, AUDIO_MONO16, AUDIO_RATE);
    cl_data_audio_enabled = 1;
}

/* Close capture and the current playback context, then disable audio. */
void shutdownaudio(void)
{
    if (cl_data_audio_capture)
        audio_alcCaptureCloseDevice()(cl_data_audio_capture);
    cl_data_audio_context = audio_alcGetCurrentContext()();
    cl_data_audio_device = audio_alcGetContextsDevice()(cl_data_audio_context);
    audio_alcMakeContextCurrent()(0);
    audio_alcDestroyContext()(cl_data_audio_context);
    audio_alcCloseDevice()(cl_data_audio_device);
    cl_data_audio_enabled = 0;
}
