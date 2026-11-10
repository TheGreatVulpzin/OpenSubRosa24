/* Sound sources, voice codecs and camera data. */

#ifndef SR_AUDIO_TYPES_H

#define SR_AUDIO_TYPES_H

#include "sr_types.h"

#pragma pack(push, 1)

/* audio_sound[]: record of 0x18 bytes. */
typedef union AudioSound { struct {
    int buffer;                          /* +0x0000 Audio source and voice frame access */
    int object;                          /* +0x0004 Audio source and voice frame access */
    int kind;                            /* +0x0008 Audio source and voice frame access */
    int keep_alive;                      /* +0x000c Audio source and voice frame access */
    uint8_t pad_0010[0x4];
    uint32_t source;                     /* +0x0014 Audio source and voice frame access */
}; uint8_t lf_raw[0x18]; } AudioSound;
SR_STATIC_ASSERT(sizeof(AudioSound) == 0x18, "AudioSound");
SR_STATIC_ASSERT(offsetof(AudioSound, buffer) == 0x0, "AudioSound.buffer");
SR_STATIC_ASSERT(offsetof(AudioSound, object) == 0x4, "AudioSound.object");
SR_STATIC_ASSERT(offsetof(AudioSound, kind) == 0x8, "AudioSound.kind");
SR_STATIC_ASSERT(offsetof(AudioSound, keep_alive) == 0xc, "AudioSound.keep_alive");
SR_STATIC_ASSERT(offsetof(AudioSound, source) == 0x14, "AudioSound.source");

/* audio_decoder[]: record of 0x28 bytes. */
typedef union AudioDecoder { struct {
    void* state;                         /* +0x0000 Audio source and voice frame access */
    uint8_t bits[36];                    /* +0x0004 Audio source and voice frame access */
}; uint8_t lf_raw[0x28]; } AudioDecoder;
SR_STATIC_ASSERT(sizeof(AudioDecoder) == 0x28, "AudioDecoder");
SR_STATIC_ASSERT(offsetof(AudioDecoder, state) == 0x0, "AudioDecoder.state");
SR_STATIC_ASSERT(offsetof(AudioDecoder, bits) == 0x4, "AudioDecoder.bits");

/* audio_encoder[]: record of 0x2c bytes. */
typedef union AudioEncoder { struct {
    void* state;                         /* +0x0000 Audio source and voice frame access */
    uint8_t bits[36];                    /* +0x0004 Audio source and voice frame access */
    void* preprocess;                    /* +0x0028 Audio source and voice frame access */
}; uint8_t lf_raw[0x2c]; } AudioEncoder;
SR_STATIC_ASSERT(sizeof(AudioEncoder) == 0x2c, "AudioEncoder");
SR_STATIC_ASSERT(offsetof(AudioEncoder, state) == 0x0, "AudioEncoder.state");
SR_STATIC_ASSERT(offsetof(AudioEncoder, bits) == 0x4, "AudioEncoder.bits");
SR_STATIC_ASSERT(offsetof(AudioEncoder, preprocess) == 0x28, "AudioEncoder.preprocess");

/* audio_channel[]: record of 0x2924c bytes. */
typedef union AudioChannel { struct {
    int active;                          /* +0x0000 Audio source and voice frame access */
    int player;                          /* +0x0004 Audio source and voice frame access */
    int human;                           /* +0x0008 Audio source and voice frame access */
    int item;                            /* +0x000c Audio source and voice frame access */
    int mode;                            /* +0x0010 Audio source and voice frame access */
    vec3 pos;                            /* +0x0014 Audio source and voice frame access */
    int16_t samples[10240];              /* +0x0020 Audio source and voice frame access */
    int head;                            /* +0x5020 Audio source and voice frame access */
    int tail;                            /* +0x5024 Audio source and voice frame access */
    int size[64];                        /* +0x5028 Audio source and voice frame access */
    int frame_mode[64];                  /* +0x5128 Audio source and voice frame access */
    uint8_t data[16384];                 /* +0x5228 Audio source and voice frame access */
    uint8_t pad_9228[0x20];
    uint32_t source;                     /* +0x9248 Audio source and voice frame access */
    uint8_t pad_924c[0x20000];
}; uint8_t lf_raw[0x2924c]; } AudioChannel;
SR_STATIC_ASSERT(sizeof(AudioChannel) == 0x2924c, "AudioChannel");
SR_STATIC_ASSERT(offsetof(AudioChannel, active) == 0x0, "AudioChannel.active");
SR_STATIC_ASSERT(offsetof(AudioChannel, player) == 0x4, "AudioChannel.player");
SR_STATIC_ASSERT(offsetof(AudioChannel, human) == 0x8, "AudioChannel.human");
SR_STATIC_ASSERT(offsetof(AudioChannel, item) == 0xc, "AudioChannel.item");
SR_STATIC_ASSERT(offsetof(AudioChannel, mode) == 0x10, "AudioChannel.mode");
SR_STATIC_ASSERT(offsetof(AudioChannel, pos) == 0x14, "AudioChannel.pos");
SR_STATIC_ASSERT(offsetof(AudioChannel, samples) == 0x20, "AudioChannel.samples");
SR_STATIC_ASSERT(offsetof(AudioChannel, head) == 0x5020, "AudioChannel.head");
SR_STATIC_ASSERT(offsetof(AudioChannel, tail) == 0x5024, "AudioChannel.tail");
SR_STATIC_ASSERT(offsetof(AudioChannel, size) == 0x5028, "AudioChannel.size");
SR_STATIC_ASSERT(offsetof(AudioChannel, frame_mode) == 0x5128, "AudioChannel.frame_mode");
SR_STATIC_ASSERT(offsetof(AudioChannel, data) == 0x5228, "AudioChannel.data");
SR_STATIC_ASSERT(offsetof(AudioChannel, source) == 0x9248, "AudioChannel.source");

/* audio_camera[]: record of 0x44 bytes. */
typedef union AudioCamera { struct {
    int player;                          /* +0x0000 Audio source and voice frame access */
    vec3 pos;                            /* +0x0004 Audio source and voice frame access */
    vec3 rot[3];                         /* +0x0010 Audio source and voice frame access */
    float projection;                    /* +0x0034 Audio source and voice frame access */
    vec3 vel;                            /* +0x0038 Audio source and voice frame access */
}; uint8_t lf_raw[0x44]; } AudioCamera;
SR_STATIC_ASSERT(sizeof(AudioCamera) == 0x44, "AudioCamera");
SR_STATIC_ASSERT(offsetof(AudioCamera, player) == 0x0, "AudioCamera.player");
SR_STATIC_ASSERT(offsetof(AudioCamera, pos) == 0x4, "AudioCamera.pos");
SR_STATIC_ASSERT(offsetof(AudioCamera, rot) == 0x10, "AudioCamera.rot");
SR_STATIC_ASSERT(offsetof(AudioCamera, projection) == 0x34, "AudioCamera.projection");
SR_STATIC_ASSERT(offsetof(AudioCamera, vel) == 0x38, "AudioCamera.vel");

#pragma pack(pop)

#endif
