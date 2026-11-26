/* Data of sound, voice and the voice chat. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

int16_t cl_data_voice_test_samples[10240];
SoundBuffer cl_data_sound_buffers[256];
int cl_data_audio_source_limit;
char cl_data_text_back[5] = "Back";
char cl_data_text_voice_minimum[14] = "Voice Minimum";
char cl_data_text_push_to_talk[17] = "Use push to talk";
char cl_data_text_push_key[17] = "Use push to talk";
char cl_data_text_voice_chat[11] = "Voice Chat";
char cl_data_text_speaking[2] = "O";
char cl_data_text_level[2] = "V";
char cl_data_text_int_format[3] = "/i";
char cl_data_text_bind_hint[49] = "Press key or mouse button for voice push to talk";
char cl_data_text_empty;
char cl_data_text_speex_frame_size[15] = "speexframesize";
void* cl_data_audio_device;
void* cl_data_audio_context;
void* cl_data_audio_capture;
int cl_data_audio_enabled;
AudioEncoder cl_data_audio_encoder;
AudioDecoder cl_data_audio_decoders[8];
AudioSound cl_data_audio_sounds[256];
int cl_data_audio_sound_count;
int cl_data_audio_objects[8];
int cl_data_audio_object_count;
int16_t cl_data_audio_play_buffer[65536];
int cl_data_audio_voice_mode;
char cl_data_audio_key_names[8192];

#endif
