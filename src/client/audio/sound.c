/* Sound sources follow vehicles and play queued voice frames.
 * The listener uses the camera position, orientation and velocity.
 * Vehicle sources occupy a packed list. Eight voice sources use separate rings.
 */
#include "audio/sound.h"
#include "audio/imports.h"
#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"
#include <string.h>

enum { AUDIO_VOICES = 8, AUDIO_OBJECTS = 8, AUDIO_FRAMES = 64,
       AUDIO_FRAME_SAMPLES = 160, AUDIO_BUFFER_FRAMES = 4, AUDIO_SAMPLE_RATE = 8000,
       AUDIO_MONO16 = 0x1101, AL_POSITION = 0x1004, AL_VELOCITY = 0x1006,
       AL_ORIENTATION = 0x100f, AL_GAIN = 0x100a, AL_PITCH = 0x1003,
       AL_STATE = 0x1010, AL_PLAYING = 0x1012, AL_PROCESSED = 0x1016,
       AL_REFERENCE_DISTANCE = 0x1020, AL_RELATIVE = 0x202,
       AUDIO_FADE = 2, AUDIO_SILENT = 3, ENGINE_LARGE = 11,
       ENGINE_TRAIN = 12, VEHICLE_SILENT = 13, AUDIO_HEAD_BONE = 2,
       AUDIO_QUEUE_MIN = 4, AUDIO_QUEUE_MAX = 24, AUDIO_QUEUE_TARGET = 8,
       AUDIO_ENGINE_SOURCE = 0, AUDIO_TIRE_SOURCE = 1 };

#define AUDIO_DISTANCE_LIMIT 65536.0f
#define AUDIO_TIRE_SCALE 8.0
#define AUDIO_TICK_RATE 60.0
#define AUDIO_VOLUME_MAX 100.0
#define AUDIO_ENGINE_PITCH_SCALE 2048.0
#define AUDIO_ENGINE_GAIN_SCALE 64.0
#define AUDIO_ENGINE_PITCH_BASE 0.25
#define AUDIO_ENGINE_OFFSET -2.5f
#define AUDIO_TIRE_OFFSET 0.125f
#define AUDIO_SOURCE_HEIGHT -0.25f
#define AUDIO_CAMERA_OFFSET -4.0f
#define AUDIO_HALF_GAIN 0.5
#define AUDIO_ENGINE_GAIN_MAX 0.5f
#define AUDIO_TIRE_GAIN_MAX 0.75f
#define AUDIO_TIRE_GAIN_MIN 0.125f
#define AUDIO_TRAIN_PITCH_MIN 0.125f
#define AUDIO_RADIO_DISTANCE 0.5f
#define AUDIO_WHISPER_DISTANCE 2.0f
#define AUDIO_TALK_DISTANCE 5.0f
#define AUDIO_YELL_DISTANCE 10.0f

/* Game functions of other files. */
#define createsoundobject CL_FN(0x44f150, int (__cdecl *)(int, const vec3 *, const vec3 *, float, float, int, int, int))

/* Remove a source when its index is valid and audio is enabled. */
void deletesound(int index)
{
    AudioSound *sounds = cl_data_audio_sounds;
    unsigned source;
    if (!cl_data_audio_enabled || index < 0 || index >= cl_data_audio_sound_count)
        return;
    source = sounds[index].source;
    audio_alSourceStop()(source);
    cl_data_audio_sound_count--;
    if (index != cl_data_audio_sound_count) {
        sounds[index] = sounds[cl_data_audio_sound_count];
        sounds[cl_data_audio_sound_count].source = source;
    }
}

/* Find a vehicle slot. A full list replaces the nearest vehicle farther than the new one. */
static int vehicle_slot(int object, const vec3 *position)
{
    int index, candidate = -1;
    float distance = vectordistance(&sr_objects[object].pos, position);
    float nearest = AUDIO_DISTANCE_LIMIT;
    for (index = 0; index < cl_data_audio_object_count; index++)
        if (cl_data_audio_objects[index] == object)
            return -1;
    if (cl_data_audio_object_count < AUDIO_OBJECTS)
        return cl_data_audio_object_count++;
    for (index = 0; index < cl_data_audio_object_count; index++) {
        int current = cl_data_audio_objects[index];
        if (current != object && distance < vectordistance(&sr_objects[current].pos, position) &&
            vectordistance(&sr_objects[current].pos, position) < nearest) {
            candidate = index;
            nearest = vectordistance(&sr_objects[current].pos, position);
        }
    }
    if (candidate != -1) {
        for (index = 0; index < cl_data_audio_sound_count; index++) {
            AudioSound *sound = &cl_data_audio_sounds[index];
            if (sound->object == cl_data_audio_objects[candidate]) {
                sound->object = -1;
                audio_alSourceStop()(sound->source);
                sound->keep_alive = 0;
            }
        }
    }
    return candidate;
}

/* Round the first tire product and each partial sum to float. */
static float tire_gain(const Object *object)
{
    return (float)((double)object->wheels[3].slip * AUDIO_TIRE_SCALE +
           (float)((double)object->wheels[2].slip * AUDIO_TIRE_SCALE +
           (float)((double)object->wheels[1].slip * AUDIO_TIRE_SCALE +
           (float)((double)object->wheels[0].slip * AUDIO_TIRE_SCALE))));
}

/* Start the engine and tire sources for vehicles in the audible list. */
static void vehicle_sounds(void)
{
    int index;
    for (index = 0; index < cl_data_audio_object_count; index++) {
        int id = cl_data_audio_objects[index];
        Object *object = &sr_objects[id];
        int engine = object->type == ENGINE_LARGE ? sr_data_soundlist.engine_large :
                     object->type == ENGINE_TRAIN ? sr_data_soundlist.engine_train : sr_data_soundlist.engine;
        float slip;
        createsoundobject(engine, &object->pos, &object->vel, AUDIO_ENGINE_GAIN_MAX, 1.0f, 1, id, AUDIO_ENGINE_SOURCE);
        slip = tire_gain(object);
        if (AUDIO_TIRE_GAIN_MIN < slip)
            createsoundobject(sr_data_soundlist.tire, &object->pos, &object->vel, slip, 1.0f, 1, id, AUDIO_TIRE_SOURCE);
    }
}

/* Update a vehicle source. Inactive vehicles stop their sources. */
static void update_vehicle_sound(AudioSound *sound, int in_vehicle)
{
    Object *object;
    vec3 position;
    float gain, pitch;
    if (sound->object == -1) return;
    object = &sr_objects[sound->object];
    if (!object->active) {
        audio_alSourceStop()(sound->source);
        sound->keep_alive = 0;
        return;
    }
    if (sound->kind != AUDIO_ENGINE_SOURCE && sound->kind != AUDIO_TIRE_SOURCE) return;
    scaleaddvectors(&position, &object->pos, &object->rot[2],
                    sound->kind == AUDIO_ENGINE_SOURCE ? AUDIO_ENGINE_OFFSET : AUDIO_TIRE_OFFSET);
    scaleaddvectors(&position, &position, &object->rot[1], AUDIO_SOURCE_HEIGHT);
    audio_alSourcefv()(sound->source, AL_POSITION, &position.x);
    scalevector(&position, &object->vel, (float)AUDIO_TICK_RATE);
    audio_alSourcefv()(sound->source, AL_VELOCITY, &position.x);
    if (sound->kind == AUDIO_ENGINE_SOURCE) {
        pitch = (float)((double)(float)object->rpm_net / AUDIO_ENGINE_PITCH_SCALE + AUDIO_ENGINE_PITCH_BASE);
        gain = (float)((double)(float)object->rpm_net / AUDIO_ENGINE_GAIN_SCALE);
        if (object->type == ENGINE_TRAIN) {
            if (1.0f < gain) gain = 1.0f;
            if (pitch < AUDIO_TRAIN_PITCH_MIN) pitch = AUDIO_TRAIN_PITCH_MIN;
            pitch = (float)((double)pitch * AUDIO_HALF_GAIN);
        } else if (AUDIO_ENGINE_GAIN_MAX < gain) gain = AUDIO_ENGINE_GAIN_MAX;
    } else {
        pitch = 1.0f;
        gain = tire_gain(object);
        if (AUDIO_TIRE_GAIN_MAX < gain) gain = AUDIO_TIRE_GAIN_MAX;
    }
    if (in_vehicle) gain = (float)((double)gain * AUDIO_HALF_GAIN);
    audio_alSourcef()(sound->source, AL_PITCH, pitch);
    audio_alSourcef()(sound->source, AL_GAIN, gain);
    if (sound->kind == AUDIO_TIRE_SOURCE && gain < AUDIO_TIRE_GAIN_MIN) {
        audio_alSourceStop()(sound->source);
        sound->keep_alive = 0;
    }
}

/* Refill processed voice buffers and restart an active voice when it stops. */
static void play_voice(int index, const vec3 *velocity)
{
    AudioChannel *voice = &cl_data_audio_channels[index];
    int queued = voice->head < voice->tail ? voice->head + AUDIO_FRAMES - voice->tail :
                                           voice->head - voice->tail;
    int processed, frame, state;
    float reference;
    unsigned buffer;
    if (queued < AUDIO_QUEUE_MIN || queued > AUDIO_QUEUE_MAX)
        voice->tail = (voice->head - AUDIO_QUEUE_TARGET) & (AUDIO_FRAMES - 1);
    audio_alGetSourcei()(voice->source, AL_PROCESSED, &processed);
    for (; processed > 0; processed--) {
        audio_alSourceUnqueueBuffers()(voice->source, 1, &buffer);
        for (frame = 0; frame < AUDIO_BUFFER_FRAMES; frame++) {
            voice->mode = voice->frame_mode[voice->tail];
            CL_FN(0x4e13c0, void (__cdecl *)(int, int, int))(index, index, voice->tail);
            memcpy(cl_data_audio_play_buffer + frame * AUDIO_FRAME_SAMPLES,
                   voice->samples + voice->tail * AUDIO_FRAME_SAMPLES,
                   AUDIO_FRAME_SAMPLES * sizeof(int16_t));
            voice->tail = (voice->tail + 1) & (AUDIO_FRAMES - 1);
        }
        audio_alBufferData()(buffer, AUDIO_MONO16, cl_data_audio_play_buffer,
                             AUDIO_BUFFER_FRAMES * AUDIO_FRAME_SAMPLES * sizeof(int16_t), AUDIO_SAMPLE_RATE);
        audio_alSourceQueueBuffers()(voice->source, 1, &buffer);
    }
    audio_alSource3f()(voice->source, AL_POSITION, voice->pos.x, voice->pos.y, voice->pos.z);
    audio_alSource3f()(voice->source, AL_VELOCITY, velocity->x, velocity->y, velocity->z);
    reference = voice->item != -1 ? AUDIO_RADIO_DISTANCE :
                voice->mode == 0 ? AUDIO_WHISPER_DISTANCE :
                voice->mode == 1 ? AUDIO_TALK_DISTANCE : AUDIO_YELL_DISTANCE;
    audio_alSourcef()(voice->source, AL_REFERENCE_DISTANCE, reference);
    audio_alSourcef()(voice->source, AL_GAIN, 1.0f);
    audio_alSourcei()(voice->source, AL_RELATIVE, 0);
    audio_alGetSourcei()(voice->source, AL_STATE, &state);
    if (!voice->active) audio_alSourceStop()(voice->source);
    else if (state != AL_PLAYING) audio_alSourcePlay()(voice->source);
}

/* Update the listener, vehicle sounds and eight voices. Disabled audio does nothing. */
void soundsimulation(const vec3 *position, const vec3 rotation[3], const vec3 *velocity)
{
    int index, state, vehicle = -1;
    vec3 orientation[2], speed;
    float gain = 1.0f;
    if (!cl_data_audio_enabled) return;
    if (cl_data_audio_camera.player != -1 && sr_players[cl_data_audio_camera.player].human_id != -1)
        vehicle = sr_humans[sr_players[cl_data_audio_camera.player].human_id].vehicle_id;
    for (index = 0; index < AUDIO_VOICES; index++) {
        AudioChannel *voice = &cl_data_audio_channels[index];
        if (!voice->active) continue;
        if (voice->human == -1 || cl_data_audio_mode == AUDIO_SILENT) {
            if (voice->item == -1)
                scaleaddvectors(&voice->pos, &cl_data_audio_camera.pos, &cl_data_audio_camera.rot[2], AUDIO_CAMERA_OFFSET);
            else voice->pos = sr_items[voice->item].pos;
        } else voice->pos = sr_humans[voice->human].bones[AUDIO_HEAD_BONE].pos;
    }
    audio_alListenerfv()(AL_POSITION, (float *)position);
    CL_FN(0x50b980, void (__cdecl *)(vec3 *, const vec3 *))(&orientation[0], &rotation[2]);
    orientation[1] = rotation[1];
    audio_alListenerfv()(AL_ORIENTATION, &orientation[0].x);
    if (!velocity) speed.x = speed.y = speed.z = 0.0f;
    else scalevector(&speed, velocity, (float)AUDIO_TICK_RATE);
    audio_alListenerfv()(AL_VELOCITY, &speed.x);
    if (cl_data_audio_mode == AUDIO_FADE) {
        gain = (float)((double)(float)cl_data_audio_fade_ticks / AUDIO_TICK_RATE);
        if (gain < 0.0f) gain = 0.0f;
        if (1.0f < gain) gain = 1.0f;
    } else if (cl_data_audio_mode == AUDIO_SILENT) gain = 0.0f;
    gain = (float)((double)((float)sr_data_option.soundvolume * gain) / AUDIO_VOLUME_MAX);
    gain = (float)(gain * gain);
    audio_alListenerf()(AL_GAIN, gain);
    for (index = 0; index < SR_MAX_OBJECT; index++) {
        Object *object = &sr_objects[index];
        if (object->active && object->type != VEHICLE_SILENT && object->rpm_net > 0) {
            int slot = vehicle_slot(index, position);
            if (slot != -1) cl_data_audio_objects[slot] = index;
        }
    }
    vehicle_sounds();
    for (index = 0; index < cl_data_audio_sound_count; index++)
        update_vehicle_sound(&cl_data_audio_sounds[index], vehicle != -1);
    index = 0;
    while (index < cl_data_audio_sound_count) {
        AudioSound *sound = &cl_data_audio_sounds[index];
        audio_alGetSourcei()(sound->source, AL_STATE, &state);
        if (state == AL_PLAYING || sound->keep_alive) index++;
        else deletesound(index);
    }
    for (index = 0; index < AUDIO_VOICES; index++)
        play_voice(index, &speed);
}
