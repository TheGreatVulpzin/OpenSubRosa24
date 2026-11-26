/* Data of start up, options and the window. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

uint8_t cl_data_auth_ticket[1024];
uint32_t cl_data_auth_ticket_size;
int cl_data_auth_ticket_ready;
int cl_data_play_ticket[4];
int cl_data_play_ticket_on;
int cl_data_fontlist[2];
WindowAtlas cl_data_texture_windowatlas;
HumanLook cl_data_texture_human[512];
int cl_data_screen_bits;
ChatMessage cl_data_chatmessage[256];
float cl_data_render_aspect;
int cl_data_render_depth_bits;
int cl_data_render_stencil_bits;
void* cl_data_gl_context;
int cl_data_steam_active;
int cl_data_player_marks[256];

#endif
