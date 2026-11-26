/* Data of keys, mouse, joystick and their labels. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

int cl_data_key_event_count;
int cl_data_key_event_next;
KeyEvent cl_data_key_events[1024];
int cl_data_message_delay;
int cl_data_num_joysticks;
Joystick cl_data_joysticks[16];
Joystick cl_data_joysticks_previous[16];
void* cl_data_joystick_handles[16];
InputMouse cl_data_input_mouse;
int cl_data_input_cursor;
int cl_data_input_cursor_previous;
int cl_data_input_fire_blocked;
int cl_data_input_chat;
uint32_t cl_data_input_chat_head;
uint32_t cl_data_input_chat_length;
char cl_data_input_chat_text[64];
char cl_data_input_label_chat;
char cl_data_input_label_pause[5] = "Exit";
char cl_data_input_label_return[15] = "Return to Game";
char cl_data_input_label_options[8] = "Options";
char cl_data_input_label_voice[14] = "Voice Options";
char cl_data_input_label_exit[16] = "Press E to exit";
char cl_data_input_label_return_connections[15] = "Return to Game";
char cl_data_input_label_empty_connection[5] = "Anon";
char cl_data_input_label_human_axis[10] = "humanaxis";
char cl_data_input_label_timer_build[14] = "buildinterior";
char cl_data_input_label_timer_optimize[17] = "optimizeinterior";
char cl_data_input_label_timer_setup[14] = "setupinterior";
char cl_data_input_label_timer_alloc[14] = "allocinterior";
char cl_data_input_label_timer_shadow[15] = "shadowinterior";
int cl_data_textinput_overwrite;
uint8_t cl_data_keyboard[512];
uint8_t cl_data_prevkeyboard[512];

#endif
