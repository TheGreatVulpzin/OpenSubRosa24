/* Voice uses narrowband frames with 160 signed samples at 8000 Hz.
 * One encoder supplies outgoing frames. Eight decoders play incoming voices.
 * Preprocessing applies noise reduction and automatic gain control.
 */
#include "audio/speex.h"
#include "audio/imports.h"
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
    encoder->state = audio_speex_encoder_init()(audio_speex_lib_get_mode()(SPEEX_NARROWBAND));
    audio_speex_encoder_ctl()(encoder->state, SPEEX_SET_QUALITY, &value);
    audio_speex_bits_init()(encoder->bits);
    encoder->preprocess = audio_speex_preprocess_state_init()(SPEEX_SAMPLES, SPEEX_RATE);
    value = 1;
    audio_speex_preprocess_ctl()(encoder->preprocess, SPEEX_SET_AGC, &value);
    audio_speex_preprocess_ctl()(encoder->preprocess, SPEEX_SET_AGC_LEVEL, &level);
    value = 1;
    audio_speex_preprocess_ctl()(encoder->preprocess, SPEEX_SET_DENOISE, &value);
    for (channel = 0; channel < SPEEX_CHANNELS; channel++) {
        AudioDecoder *decoder = &cl_data_audio_decoders[channel];
        decoder->state = audio_speex_decoder_init()(audio_speex_lib_get_mode()(SPEEX_NARROWBAND));
        audio_speex_bits_init()(decoder->bits);
    }
}

/* Release all codec states and their bit buffers. */
void disablespeex(void)
{
    int channel;
    AudioEncoder *encoder = &cl_data_audio_encoder;
    audio_speex_encoder_destroy()(encoder->state);
    audio_speex_bits_destroy()(encoder->bits);
    audio_speex_preprocess_state_destroy()(encoder->preprocess);
    for (channel = 0; channel < SPEEX_CHANNELS; channel++) {
        audio_speex_decoder_destroy()(cl_data_audio_decoders[channel].state);
        audio_speex_bits_destroy()(cl_data_audio_decoders[channel].bits);
    }
}

/* Apply preprocessing to one frame. The channel argument does not select the state. */
void preprocessspeex(int channel, int player, int frame)
{
    (void)channel;
    audio_speex_preprocess_run()(cl_data_audio_encoder.preprocess,
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
        cl_data_audio_samples[sample] = (float)ring->samples[frame][sample];
    audio_speex_bits_reset()(encoder->bits);
    audio_speex_encode()(encoder->state, cl_data_audio_samples, encoder->bits);
    ring->size[frame] = audio_speex_bits_write()(encoder->bits, (char *)ring->data[frame], SPEEX_BYTES);
}
