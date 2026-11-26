/* Data of the menus and the front end. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

Font cl_data_fonts[4];
int cl_data_landsector_vbo[1024];
int cl_data_sector_shadow_vbo;
VideoMode cl_data_video_modes[4096];
int cl_data_video_mode_count;
int cl_data_menu_item_index;
int cl_data_textinput_start;
int cl_data_textinput_end;
char cl_data_textinput_text[16384];
char cl_data_textinput_shown[16384];
float cl_data_menu_vertices[32];
float cl_data_menu_texcoords[32];
float cl_data_menu_alpha[16];
int cl_data_menu_quad_count;
int cl_data_menu_quads[64];
int cl_data_wrap_line_ends[64];
int cl_data_client_account_id[2];
int cl_data_wrap_line_count;
int cl_data_screen_fullscreen;
char cl_data_joystick_names[2560];
JoinServer cl_data_join_servers[1024];
int cl_data_join_server_count;
JoinAddress cl_data_join_addresses[8192];
int cl_data_join_address_count;
int cl_data_join_master_first;
float cl_data_join_columns[10];
int cl_data_vbo_next;
int cl_data_framebuffer_ready;
int cl_data_vbo_spare_a;
int cl_data_vbo_spare_b;
int cl_data_vbo_spare_c;
char cl_data_version_marks[2];

#endif
