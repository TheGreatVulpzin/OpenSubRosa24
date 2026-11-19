/* The two sound tests of the client: a WAV file that goes through the voice encoder, and a ring of
 * recorded voice frames that plays back through the decoder.
 */
#include "audio/test_voice.h"

#include <stdint.h>
#include <string.h>

#include "audio/imports.h"
#include "audio/speex.h"
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { AL_FORMAT_MONO16 = 0x1101, VOICE_FRAMES = 64, VOICE_FRAME_SAMPLES = 160, VOICE_FRAME_MASK = 0x3f,
       VOICE_RATE = 8000, VOICE_BYTES = 0x5000, ENCODE_RING = 1, STAGE_RING = 1, RAW_RING = 2, LOOK_RING = 3, VOICE_TEST_KEY = 225 };

/* The audio spec of SDL: 32 bytes. The loader fills all of them. */
typedef struct TestSpec {
    int frequency;
    uint16_t format;
    uint8_t channels;
    uint8_t rest[25];
} TestSpec;

/* Load a mono WAV file of the sound folder, cut it into 64 frames of 160 samples, encode each frame
 * and give the sound buffer the whole file. With mode 0 the frames go to the voice ring of player 1
 * and the encoder reads each frame by its number. With another mode the encoder always reads
 * frame 0. */
void __cdecl testspeex(int sound, const char *file, int mode)
{
    SoundBuffer *record = &cl_data_sound_buffers[sound];
    int format = AL_FORMAT_MONO16;
    TestSpec spec;
    unsigned char *data;
    unsigned length = 0;
    int changed, frame, i;
    void *rw;

    spec.frequency = 0;
    if (cl_data_audio_enabled == 0)
        return;
    changed = os_chdir("sound");
    rw = audio_SDL_RWFromFile()(file, "rb");
    if (audio_SDL_LoadWAV_RW()(rw, 1, &spec, &data, &length) != NULL && spec.channels == 1) {
        memcpy(cl_data_voice_test_samples, data, length);
        format = AL_FORMAT_MONO16;
        audio_SDL_FreeWAV()(data);
    }
    if (changed == 0)
        os_chdir("..");
    for (frame = 0; frame < VOICE_FRAMES; frame++) {
        for (i = 0; i < VOICE_FRAME_SAMPLES; i++)
            sr_data_speex.voice_ring[STAGE_RING].samples[frame][i] =
                cl_data_voice_test_samples[frame * VOICE_FRAME_SAMPLES + i];
        if (mode == 0)
            encodespeex(0, ENCODE_RING, frame & VOICE_FRAME_MASK);
        else
            encodespeex(0, 0, 0);
    }
    audio_alBufferData()(record->buffer, format, cl_data_voice_test_samples, (int)length, spec.frequency);
    record->loaded = 1;
}

/* Play the voice ring of a player back through a sound buffer. The 64 frames are copied to ring 1
 * with their sizes, and the frames are read in turn. With the voice key up the samples of the
 * ring come as they are. With the key down each frame goes through the decoder first, and the raw
 * and the decoded samples are kept in rings 2 and 3. The sum of the frame sizes goes to the debug
 * field that the test shares. */
void __cdecl testvoice(int sound, int player)
{
    SoundBuffer *record = &cl_data_sound_buffers[sound];
    VoiceRing *stage = &sr_data_speex.voice_ring[STAGE_RING];
    const VoiceRing *source = &sr_data_speex.voice_ring[player];
    int frame, i;

    sr_data_debug.shown_team = 0;
    for (frame = 0; frame < VOICE_FRAMES; frame++) {
        memcpy(stage->data[frame], source->data[frame], (size_t)source->size[frame]);
        stage->size[frame] = source->size[frame];
        sr_data_debug.shown_team += source->size[frame];
    }
    stage->head = 0;
    for (frame = 0; frame < VOICE_FRAMES; frame++) {
        int head = stage->head;

        if (cl_data_audio_keys[VOICE_TEST_KEY] == 0) {
            for (i = 0; i < VOICE_FRAME_SAMPLES; i++)
                cl_data_voice_test_samples[frame * VOICE_FRAME_SAMPLES + i] = source->samples[head][i];
        } else {
            decodespeex(0, 1, head);
            for (i = 0; i < VOICE_FRAME_SAMPLES; i++) {
                sr_data_speex.voice_ring[RAW_RING].samples[head][i] = source->samples[head][i];
                sr_data_speex.voice_ring[LOOK_RING].samples[head][i] = sr_data_voice[1].samples[head][i];
                cl_data_voice_test_samples[frame * VOICE_FRAME_SAMPLES + i] =
                    sr_data_voice[1].samples[head][i];
            }
        }
        stage->head = (head + 1) & VOICE_FRAME_MASK;
    }
    audio_alBufferData()(record->buffer, AL_FORMAT_MONO16, cl_data_voice_test_samples, VOICE_BYTES, VOICE_RATE);
    record->loaded = 1;
}
