/* Voice uses narrowband frames with 160 signed samples at 8000 Hz.
 * One encoder supplies outgoing frames. Eight decoders play incoming voices.
 * Preprocessing applies noise reduction and automatic gain control.
 */
#include "audio/speex.h"
#include "system_libs.h"
#include "sr_runtime.h"

enum { SPEEX_SAMPLES = 160, SPEEX_RATE = 8000, SPEEX_CHANNELS = 8,
       SPEEX_BYTES = 200, SPEEX_SET_QUALITY = 4, SPEEX_SET_AGC = 2,
       SPEEX_SET_AGC_LEVEL = 6, SPEEX_SET_DENOISE = 0,
       SPEEX_NARROWBAND = 0, SPEEX_QUALITY = 8 };
#define SPEEX_AGC_LEVEL 16000.0f

/* Create the encoder, preprocessing state and eight decoders. */
void enablespeex(void)
{
    int value = SPEEX_QUALITY, channel;
    float level = SPEEX_AGC_LEVEL;
    AudioEncoder *encoder = &cl_data_audio_encoder;
    encoder->state = speex_encoder_init(speex_lib_get_mode(SPEEX_NARROWBAND));
    speex_encoder_ctl(encoder->state, SPEEX_SET_QUALITY, &value);
    speex_bits_init(encoder->bits);
    encoder->preprocess = speex_preprocess_state_init(SPEEX_SAMPLES, SPEEX_RATE);
    value = 1;
    speex_preprocess_ctl(encoder->preprocess, SPEEX_SET_AGC, &value);
    speex_preprocess_ctl(encoder->preprocess, SPEEX_SET_AGC_LEVEL, &level);
    value = 1;
    speex_preprocess_ctl(encoder->preprocess, SPEEX_SET_DENOISE, &value);
    for (channel = 0; channel < SPEEX_CHANNELS; channel++) {
        AudioDecoder *decoder = &cl_data_audio_decoders[channel];
        decoder->state = speex_decoder_init(speex_lib_get_mode(SPEEX_NARROWBAND));
        speex_bits_init(decoder->bits);
    }
}

/* Release all codec states and their bit buffers. */
void disablespeex(void)
{
    int channel;
    AudioEncoder *encoder = &cl_data_audio_encoder;
    speex_encoder_destroy(encoder->state);
    speex_bits_destroy(encoder->bits);
    speex_preprocess_state_destroy(encoder->preprocess);
    for (channel = 0; channel < SPEEX_CHANNELS; channel++) {
        speex_decoder_destroy(cl_data_audio_decoders[channel].state);
        speex_bits_destroy(cl_data_audio_decoders[channel].bits);
    }
}

/* Apply preprocessing to one frame. The channel argument does not select the state. */
void preprocessspeex(int channel, int player, int frame)
{
    (void)channel;
    speex_preprocess_run(cl_data_audio_encoder.preprocess,
                               sr_data_speex.voice_ring[player].samples[frame]);
}

/* Encode one frame and store its byte count, up to 200 bytes. */
void encodespeex(int channel, int player, int frame)
{
    int sample;
    AudioEncoder *encoder = &cl_data_audio_encoder;
    VoiceRing *ring = &sr_data_speex.voice_ring[player];
    (void)channel;
    for (sample = 0; sample < SPEEX_SAMPLES; sample++)
        sr_data_speex.samples[sample] = (float)ring->samples[frame][sample];
    speex_bits_reset(encoder->bits);
    speex_encode(encoder->state, sr_data_speex.samples, encoder->bits);
    ring->size[frame] = speex_bits_write(encoder->bits, (char *)ring->data[frame], SPEEX_BYTES);
}

/* Decode one frame of a speaker. A frame with no bytes gives silence. A decoded frame clears its byte
 * count. Every 32nd sample of the frame goes to the level table of the speaker. */
void decodespeex(int decoder_index, int channel_index, int frame)
{
    AudioDecoder *decoder = &cl_data_audio_decoders[decoder_index];
    VoiceChannelClient *channel = &sr_data_voice[channel_index];
    int sample;

    if (channel->size[frame] == 0) {
        for (sample = 0; sample < SPEEX_SAMPLES; sample++)
            channel->samples[frame][sample] = 0;
    } else {
        speex_bits_read_from(decoder->bits, (char *)channel->data[frame], channel->size[frame]);
        speex_decode(decoder->state, decoder->bits, sr_data_speex.samples);
        for (sample = 0; sample < SPEEX_SAMPLES; sample++)
            channel->samples[frame][sample] = (int16_t)(long long)sr_data_speex.samples[sample];
        channel->size[frame] = 0;
    }
    for (sample = 0; sample < SPEEX_SAMPLES; sample += 32)
        channel->level[frame][sample / 32] = channel->samples[frame][sample];
}
