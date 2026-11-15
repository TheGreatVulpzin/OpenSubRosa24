/* The OpenAL buffer of a sound number: loaded from a WAV file, or made from waves. */
#include "audio/wav.h"

#include <math.h>
#include <stdint.h>

#include "audio/imports.h"
#include "client.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { AL_FORMAT_MONO8 = 0x1100, AL_FORMAT_MONO16 = 0x1101, AUDIO_S8 = 0x8008,
       TONE_SAMPLES = 0x4000, NOISE_SAMPLES = 0x10000, SAMPLE_RATE = 44100, SAMPLE_RATE_HZ = 0xac44 };

#define g_soundlist sr_data_soundlist

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

/* The two tones of the dial pad key: the column and the row, in Hz. Key 0 is the fourth row. */
static void key_tones(int key, float *column, float *row)
{
    *column = 0.0f;
    *row = 0.0f;
    if (key == 1 || key == 4 || key == 7)
        *column = 1209.0f;
    if (key == 2 || key == 5 || key == 8 || key == 0)
        *column = 1336.0f;
    if (key == 3 || key == 6 || key == 9)
        *column = 1477.0f;
    if (key == 1 || key == 2 || key == 3)
        *row = 697.0f;
    if (key == 4 || key == 5 || key == 6)
        *row = 770.0f;
    if (key == 7 || key == 8 || key == 9)
        *row = 852.0f;
    if (key == 0)
        *row = 941.0f;
}

/* One step of four chained one pole filters with a cubic limit on the last one. The input is a
 * sample of 16 bits. The state is four floats. The result is the last stage. */
static float noise_filter(float state[4], int sample)
{
    state[0] = (float)((((float)((float)((float)sample / 32768.0) - (float)(state[3] * 1.0 * 4.0))) - state[0]) *
                           0.02500000037252903 + state[0]);
    state[1] = (float)((state[0] - state[1]) * 0.02500000037252903 + state[1]);
    state[2] = (float)((state[1] - state[2]) * 0.02500000037252903 + state[2]);
    state[3] = (float)((state[2] - state[3]) * 0.02500000037252903 + state[3]);
    state[3] = (float)(state[3] - ((double)state[3] * state[3] * state[3]) / 6.0);
    return state[3];
}

/* Make the buffer of a sound number from waves. A dial pad key gets its two tones added: 0x4000
 * bytes at 44100 Hz. The sound of the engine_train number gets a filtered noise: a saw wave, four
 * low pass stages with a soft cubic limit, and a gain of 3, in 0x20000 bytes. The reference
 * distance is set for every number. The waves go through audio_play_buffer. */
void __cdecl createwav(int sound, float reference)
{
    int16_t *wave = cl_data_audio_play_buffer;
    int i;

    if (g_soundlist.digit[0] <= sound && sound <= g_soundlist.digit[9]) {
        float phase_column = 0.0f;
        float phase_row = 0.0f;
        float column;
        float row;

        key_tones(sound - g_soundlist.digit[0], &column, &row);
        for (i = 0; i < TONE_SAMPLES; i++) {
            wave[i] = (int16_t)cl_ftoi(x87_sin((double)phase_column) * 1.0 * 8192.0);
            wave[i] = (int16_t)cl_ftoi((double)(int)wave[i] + x87_sin((double)phase_row) * 1.0 * 8192.0);
            phase_column = (float)(phase_column + (float)(((column + column) * 3.14159265359) / SAMPLE_RATE));
            phase_row = (float)(phase_row + (float)(((row + row) * 3.14159265359) / SAMPLE_RATE));
        }
        audio_alBufferData()(cl_data_sound_buffers[sound].buffer, AL_FORMAT_MONO16, wave, TONE_SAMPLES,
                             SAMPLE_RATE_HZ);
        cl_data_sound_buffers[sound].loaded = 1;
    }
    cl_data_sound_buffers[sound].reference = reference;
    if (sound == g_soundlist.engine_train) {
        float phase = 0.0f;
        float state[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        for (i = 0; i < NOISE_SAMPLES; i++) {
            double whole = floor((double)phase);

            wave[i] = (int16_t)cl_ftoi(fabs((double)phase - whole) * 16384.0 - 8192.0);
            phase = (float)(phase + 9.765625E-4);
        }
        for (i = 0; i < NOISE_SAMPLES; i++) {
            float last = noise_filter(state, wave[i]);

            if (i > TONE_SAMPLES - 1)
                wave[i] = (int16_t)cl_ftoi(last * 65536.0);
        }
        for (i = 0; i < TONE_SAMPLES; i++)
            wave[i] = (int16_t)cl_ftoi(noise_filter(state, wave[i]) * 65536.0);
        for (i = 0; i < NOISE_SAMPLES; i++)
            wave[i] = (int16_t)(wave[i] * 3);
        audio_alBufferData()(cl_data_sound_buffers[sound].buffer, AL_FORMAT_MONO16, wave, NOISE_SAMPLES * 2,
                             SAMPLE_RATE_HZ);
        cl_data_sound_buffers[sound].loaded = 1;
    }
}
