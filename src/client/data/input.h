/* Declarations of the data of keys, mouse, joystick and their labels. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_INPUT_H
#define CL_DATA_INPUT_H
#ifndef SR_STORAGE_GUARDS
extern int cl_data_key_event_count;
extern int cl_data_key_event_next;
extern KeyEvent cl_data_key_events[1024];
extern int cl_data_message_delay;
extern int cl_data_num_joysticks;
extern Joystick cl_data_joysticks[16];
extern Joystick cl_data_joysticks_previous[16];
extern void* cl_data_joystick_handles[16];
extern InputMouse cl_data_input_mouse;
extern int cl_data_input_cursor;
extern int cl_data_input_cursor_previous;
extern int cl_data_input_fire_blocked;
extern int cl_data_input_chat;
extern uint32_t cl_data_input_chat_head;
extern uint32_t cl_data_input_chat_length;
extern char cl_data_input_chat_text[64];
extern char cl_data_input_label_chat;
extern char cl_data_input_label_pause[5];
extern char cl_data_input_label_return[15];
extern char cl_data_input_label_options[8];
extern char cl_data_input_label_voice[14];
extern char cl_data_input_label_exit[16];
extern char cl_data_input_label_return_connections[15];
extern char cl_data_input_label_empty_connection[5];
extern char cl_data_input_label_human_axis[10];
extern char cl_data_input_label_timer_build[14];
extern char cl_data_input_label_timer_optimize[17];
extern char cl_data_input_label_timer_setup[14];
extern char cl_data_input_label_timer_alloc[14];
extern char cl_data_input_label_timer_shadow[15];
extern int cl_data_textinput_overwrite;
extern uint8_t cl_data_keyboard[512];
extern uint8_t cl_data_prevkeyboard[512];
#endif
#endif
