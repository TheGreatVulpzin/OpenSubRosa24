/* The OpenAL buffers of the sounds, and the start of a sound on a free source.
 *
 * Each sound number has one buffer and one record in sound_buffers. A live sound has a record
 * in audio_sounds with its own source. The list of live sounds is packed: a new sound goes at
 * the end, and audio_source_limit is the number of sources that OpenAL made.
 */
#include "audio/sound_create.h"

#include <stddef.h>

#include "system_libs.h"
#include "client.h"
#include "math/small_vectors.h"
#include "sr_runtime.h"
#include "vectors.h"

enum { SOUND_BUFFERS = 0x100, CHANNELS = 8, CHANNEL_BUFFERS = 4, SOURCE_TRIES = 0x20, SOUNDS_PER_BUFFER = 8,
       AL_PITCH = 0x1003, AL_POSITION = 0x1004, AL_LOOPING = 0x1007, AL_VELOCITY = 0x1006,
       AL_BUFFER = 0x1009, AL_GAIN = 0x100a, AL_REFERENCE_DISTANCE = 0x1020 };

#define SOUND_VELOCITY_SCALE 60.0f
#define CREATESOUND_RANGE 512.0

/* Make the buffers and the sources. A sound number gets one buffer, a voice channel gets four.
 * A voice source that OpenAL cannot make keeps the id -1. The sound sources come one at a time
 * until OpenAL reports an error: the number made is the limit of live sounds. */
void __cdecl setupsounds(void)
{
    int i;
    int k;

    if (cl_data_audio_enabled == 0)
        return;
    for (i = 0; i < SOUND_BUFFERS; i++)
        alGenBuffers(1, &cl_data_sound_buffers[i].buffer);
    for (i = 0; i < CHANNELS; i++)
        for (k = 0; k < CHANNEL_BUFFERS; k++)
            alGenBuffers(1, &((AudioChannel *)sr_data_voice)[i].buffers[k]);
    for (i = 0; i < CHANNELS; i++) {
        alGenSources(1, &((AudioChannel *)sr_data_voice)[i].source);
        if (alGetError() != 0)
            ((AudioChannel *)sr_data_voice)[i].source = 0xffffffffu;
    }
    cl_data_audio_source_limit = 0;
    alGenSources(1, &cl_data_audio_sounds[0].source);
    for (i = 0; i < SOURCE_TRIES; i++) {
        if (alGetError() != 0)
            return;
        alGenSources(1, &cl_data_audio_sounds[i + 1].source);
        cl_data_audio_source_limit++;
    }
}

/* Limit a value to a range. A value that is not a number stays as it is. */
static float clamp_value(float value, float low, float high)
{
    if (value < low)
        value = low;
    if (high < value)
        value = high;
    return value;
}

/* Give the new sound its source: the buffer, the looping flag, the place, the velocity (the vector
 * times 60, or zero), the pitch, the gain and the reference distance. Then play it. */
static void start_source(const AudioSound *sound, int buffer, const vec3 *position, const vec3 *velocity,
                         float gain, float pitch, int looping)
{
    vec3 scaled;

    alSourcei(sound->source, AL_BUFFER, (int)cl_data_sound_buffers[buffer].buffer);
    alSourcei(sound->source, AL_LOOPING, looping);
    alSourcefv(sound->source, AL_POSITION, (float *)position);
    if (velocity == NULL)
        zerovector((unsigned int *)&scaled);
    else
        scalevector(&scaled, velocity, SOUND_VELOCITY_SCALE);
    alSourcefv(sound->source, AL_VELOCITY, &scaled.x);
    alSourcef(sound->source, AL_PITCH, pitch);
    alSourcef(sound->source, AL_GAIN, gain);
    alSourcef(sound->source, AL_REFERENCE_DISTANCE, cl_data_sound_buffers[buffer].reference);
    alSourcePlay(sound->source);
}

/* Play a sound on a free source. Return its index, or -1 when it cannot play. A call with an
 * object returns the index of the sound that already plays for that object and kind. A call
 * with object -1 refuses a ninth sound of the same buffer. The gain goes from 0 to 1 and the
 * pitch from 0 to 2. */
int __cdecl createsoundobject(int buffer, const vec3 *position, const vec3 *velocity, float gain, float pitch,
                              int keep_alive, int object, int kind)
{
    AudioSound *sound;
    int i;

    if (cl_data_audio_enabled == 0 || cl_data_sound_buffers[buffer].loaded == 0)
        return -1;
    if (cl_data_audio_sound_count >= cl_data_audio_source_limit)
        return -1;
    if (object == -1) {
        int same = 0;

        for (i = 0; i < cl_data_audio_sound_count; i++)
            if (cl_data_audio_sounds[i].buffer == buffer)
                same++;
        if (same > SOUNDS_PER_BUFFER - 1)
            return -1;
    } else {
        for (i = 0; i < cl_data_audio_sound_count; i++)
            if (cl_data_audio_sounds[i].object == object && cl_data_audio_sounds[i].kind == kind)
                return i;
    }
    gain = clamp_value(gain, 0.0f, 1.0f);
    pitch = clamp_value(pitch, 0.0f, 2.0f);
    sound = &cl_data_audio_sounds[cl_data_audio_sound_count];
    sound->buffer = buffer;
    sound->keep_alive = keep_alive;
    sound->object = object;
    sound->kind = kind;
    start_source(sound, buffer, position, velocity, gain, pitch, keep_alive);
    return cl_data_audio_sound_count++;
}

/* Play a sound with no object and no looping. The sound must start within 512 units of the
 * listener. A ninth sound of the same buffer is refused. The kind of the record does not change. */
int __cdecl createsound(int buffer, const vec3 *position, const vec3 *velocity, float gain, float pitch)
{
    AudioSound *sound;
    int same = 0;
    int i;

    if (cl_data_audio_enabled == 0 || cl_data_sound_buffers[buffer].loaded == 0)
        return -1;
    if (cl_data_audio_sound_count >= cl_data_audio_source_limit)
        return -1;
    for (i = 0; i < cl_data_audio_sound_count; i++)
        if (cl_data_audio_sounds[i].buffer == buffer)
            same++;
    if (same >= SOUNDS_PER_BUFFER)
        return -1;
    if (!((double)vectordistance(position, &cl_data_audio_camera.pos) <= CREATESOUND_RANGE))
        return -1;
    gain = clamp_value(gain, 0.0f, 1.0f);
    pitch = clamp_value(pitch, 0.0f, 2.0f);
    sound = &cl_data_audio_sounds[cl_data_audio_sound_count];
    sound->buffer = buffer;
    sound->keep_alive = 0;
    sound->object = -1;
    start_source(sound, buffer, position, velocity, gain, pitch, 0);
    return cl_data_audio_sound_count++;
}
