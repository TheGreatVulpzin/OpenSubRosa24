/* Declarations of the data of start up, options and the window. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_SETUP_H
#define CL_DATA_SETUP_H
#ifndef SR_STORAGE_GUARDS
extern uint8_t cl_data_auth_ticket[1024];
extern uint32_t cl_data_auth_ticket_size;
extern int cl_data_auth_ticket_ready;
extern int cl_data_play_ticket[4];
extern int cl_data_play_ticket_on;
extern int cl_data_fontlist[2];
extern WindowAtlas cl_data_texture_windowatlas;
extern HumanLook cl_data_texture_human[512];
extern int cl_data_screen_bits;
extern ChatMessage cl_data_chatmessage[256];
extern float cl_data_render_aspect;
extern int cl_data_render_depth_bits;
extern int cl_data_render_stencil_bits;
extern void* cl_data_gl_context;
extern int cl_data_steam_active;
extern int cl_data_player_marks[256];
#endif
#endif
