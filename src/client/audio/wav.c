/* Load a WAV file of the sound folder into the OpenAL buffer of a sound number. */
#include "audio/wav.h"

#include <stdint.h>

#include "audio/imports.h"
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { AL_FORMAT_MONO8 = 0x1100, AL_FORMAT_MONO16 = 0x1101, AUDIO_S8 = 0x8008 };

/* The audio spec of SDL: 32 bytes. The loader fills all of them. */
typedef struct WavSpec {
    int frequency;
    uint16_t format;
    uint8_t channels;
    uint8_t rest[25];
} WavSpec;

/* Load the file of the sound folder. Only a mono file loads. A file of 8 bit samples uses the
 * 8 bit format, and every other file uses the 16 bit format. The record keeps the reference
 * distance and the loaded flag becomes 1. The folder changes back only when it changed. */
void __cdecl loadwav(int sound, const char *path, float reference)
{
    SoundBuffer *record = &cl_data_sound_buffers[sound];
    int changed;
    void *file;
    WavSpec spec;
    unsigned char *data;
    unsigned length;

    if (cl_data_audio_enabled == 0)
        return;
    changed = os_chdir("sound");
    file = audio_SDL_RWFromFile()(path, "rb");
    if (audio_SDL_LoadWAV_RW()(file, 1, &spec, &data, &length) != NULL && spec.channels == 1) {
        int format = (spec.format == 8 || spec.format == AUDIO_S8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;

        audio_alBufferData()(record->buffer, format, data, (int)length, spec.frequency);
        audio_SDL_FreeWAV()(data);
        record->reference = reference;
        record->loaded = 1;
    }
    if (changed == 0)
        os_chdir("..");
}
