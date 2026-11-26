/* Declarations of the data of sound, voice and the voice chat. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_AUDIO_H
#define CL_DATA_AUDIO_H
#ifndef SR_STORAGE_GUARDS
extern int16_t cl_data_voice_test_samples[10240];
extern SoundBuffer cl_data_sound_buffers[256];
extern int cl_data_audio_source_limit;
extern char cl_data_text_back[5];
extern char cl_data_text_voice_minimum[14];
extern char cl_data_text_push_to_talk[17];
extern char cl_data_text_push_key[17];
extern char cl_data_text_voice_chat[11];
extern char cl_data_text_speaking[2];
extern char cl_data_text_level[2];
extern char cl_data_text_int_format[3];
extern char cl_data_text_bind_hint[49];
extern char cl_data_text_empty;
extern char cl_data_text_speex_frame_size[15];
extern void* cl_data_audio_device;
extern void* cl_data_audio_context;
extern void* cl_data_audio_capture;
extern int cl_data_audio_enabled;
extern AudioEncoder cl_data_audio_encoder;
extern AudioDecoder cl_data_audio_decoders[8];
extern AudioSound cl_data_audio_sounds[256];
extern int cl_data_audio_sound_count;
extern int cl_data_audio_objects[8];
extern int cl_data_audio_object_count;
extern int16_t cl_data_audio_play_buffer[65536];
extern int cl_data_audio_voice_mode;
extern char cl_data_audio_key_names[8192];
#endif
#endif
