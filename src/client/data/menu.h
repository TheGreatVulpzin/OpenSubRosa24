/* Declarations of the data of the menus and the front end. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_MENU_H
#define CL_DATA_MENU_H
#ifndef SR_STORAGE_GUARDS
extern Font cl_data_fonts[4];
extern int cl_data_landsector_vbo[1024];
extern int cl_data_sector_shadow_vbo;
extern VideoMode cl_data_video_modes[4096];
extern int cl_data_video_mode_count;
extern int cl_data_menu_item_index;
extern int cl_data_textinput_start;
extern int cl_data_textinput_end;
extern char cl_data_textinput_text[16384];
extern char cl_data_textinput_shown[16384];
extern float cl_data_menu_vertices[32];
extern float cl_data_menu_texcoords[32];
extern float cl_data_menu_alpha[16];
extern int cl_data_menu_quad_count;
extern int cl_data_menu_quads[64];
extern int cl_data_wrap_line_ends[64];
extern int cl_data_client_account_id[2];
extern int cl_data_wrap_line_count;
extern int cl_data_screen_fullscreen;
extern char cl_data_joystick_names[2560];
extern JoinServer cl_data_join_servers[1024];
extern int cl_data_join_server_count;
extern JoinAddress cl_data_join_addresses[8192];
extern int cl_data_join_address_count;
extern int cl_data_join_master_first;
extern float cl_data_join_columns[10];
extern int cl_data_vbo_next;
extern int cl_data_framebuffer_ready;
extern int cl_data_vbo_spare_a;
extern int cl_data_vbo_spare_b;
extern int cl_data_vbo_spare_c;
extern char cl_data_version_marks[2];
#endif
#endif
