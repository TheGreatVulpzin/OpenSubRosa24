/* Each listener has eight speaker slots.
 * Free slots take priority over distance comparisons.
 * Capture stores 160-sample frames in a ring of 64 entries.
 */
#include "audio/voice.h"
#include "audio/speex.h"
#include "audio/imports.h"
#include "client.h"
#include "sr_runtime.h"

enum { VOICE_SLOTS = 8, VOICE_SAMPLES = 160, VOICE_FRAMES = 64,
       CAPTURE_BATCH = 16, SILENCE_LIMIT = 64, VOICE_RELEASE = 16, TALK_RELEASE = 24,
       SAMPLE_LIMIT = 32767, ALC_CAPTURE_SAMPLES = 0x312,
       VOICE_FRAME_SIZE_LABEL = 0x1263b0 };
#define VOICE_DISTANCE_LIMIT 65536.0f

/* Choose a free slot or a replacement. Return -1 when no slot qualifies. */
int getnextvoice(int listener, float distance)
{
    Connection *connection = &sr_data_connection[listener];
    float nearest = VOICE_DISTANCE_LIMIT;
    int slot, candidate = -1;
    for (slot = 0; slot < VOICE_SLOTS; slot++)
        if (!connection->voice[slot].active)
            return slot;
    /* The first comparison reads the key before the slot array.
     * Later comparisons read the distance of the current candidate. */
    for (slot = 0; slot < VOICE_SLOTS; slot++) {
        float current = candidate == -1 ? connection->voice_key : connection->voice[candidate].distance;
        if (current < nearest) {
            candidate = slot;
            nearest = connection->voice[slot].distance;
        }
    }
    return candidate != -1 && distance < nearest ? candidate : -1;
}

/* Add a speaker when a free slot or a replacement exists. */
void addvoice(int listener, int player, int human, int item, float distance)
{
    int index = getnextvoice(listener, distance);
    if (index != -1) {
        VoiceSlot *slot = &sr_data_connection[listener].voice[index];
        slot->active = 1;
        slot->player = player;
        slot->human = human;
        slot->item = item;
        slot->distance = distance;
    }
}

/* Capture at most sixteen frames. A missing capture device leaves the ring unchanged. */
void getaudioinput(int player)
{
    VoiceRing *ring = &sr_data_speex.voice_ring[player];
    int available, batch, sample;
    if (!cl_data_audio_voice_enabled) {
        for (sample = 0; sample < VOICE_FRAMES; sample++)
            ring->size[sample] = 0;
        ring->voice_off = 1;
        return;
    }
    if (!cl_data_audio_capture)
        return;
    audio_alcGetIntegerv()(cl_data_audio_capture, ALC_CAPTURE_SAMPLES, sizeof(int), &available);
    for (batch = 0; available >= VOICE_SAMPLES && batch < CAPTURE_BATCH; batch++) {
        int quiet = 1;
        int frame = ring->head;
        audio_alcCaptureSamples()(cl_data_audio_capture, ring->samples[frame], VOICE_SAMPLES);
        for (sample = 0; sample < VOICE_SAMPLES; sample++) {
            int value = (int)((double)(float)ring->samples[frame][sample] * cl_data_audio_voice_gain);
            if (value < -SAMPLE_LIMIT) value = -SAMPLE_LIMIT;
            if (value > SAMPLE_LIMIT) value = SAMPLE_LIMIT;
            if (value < -cl_data_audio_threshold || value > cl_data_audio_threshold)
                quiet = 0;
            ring->samples[frame][sample] = (int16_t)value;
        }
        preprocessspeex(0, player, frame);
        if (!cl_data_audio_push_to_talk) {
            if (!quiet) ring->silence_ticks = 0;
            else if (ring->silence_ticks < SILENCE_LIMIT) ring->silence_ticks++;
            if (ring->silence_ticks < VOICE_RELEASE) quiet = 0;
        } else if (!cl_data_audio_input_blocked &&
                   (cl_data_audio_keys[cl_data_audio_voice_key] ||
                    (cl_data_audio_mouse_buttons & cl_data_audio_voice_mouse))) {
            quiet = 0;
            ring->silence_ticks = 0;
        } else {
            quiet = 0;
            if (ring->silence_ticks < SILENCE_LIMIT) ring->silence_ticks++;
            if (ring->silence_ticks >= TALK_RELEASE) quiet = 1;
        }
        if (quiet) {
            for (sample = 0; sample < VOICE_SAMPLES; sample++) {
                cl_data_audio_capture_backup[frame * VOICE_SAMPLES + sample] = ring->samples[frame][sample];
                ring->samples[frame][sample] = 0;
            }
        }
        encodespeex(0, player, frame);
        CL_FN(0x46aaa0, void (__cdecl *)(char *, int))(
            (char *)(sr_base + VOICE_FRAME_SIZE_LABEL), ring->size[frame]);
        ring->mode[frame] = cl_data_audio_voice_mode;
        if (quiet) {
            for (sample = 0; sample < VOICE_SAMPLES; sample++)
                ring->samples[frame][sample] = cl_data_audio_capture_backup[frame * VOICE_SAMPLES + sample];
            ring->size[frame] = 0;
        }
        ring->voice_off = quiet;
        ring->head = (ring->head + 1) & (VOICE_FRAMES - 1);
        audio_alcGetIntegerv()(cl_data_audio_capture, ALC_CAPTURE_SAMPLES, sizeof(int), &available);
    }
}
