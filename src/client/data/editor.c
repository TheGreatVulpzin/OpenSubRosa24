/* Data of the building, level and model editors. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

int cl_data_building_click_wait;
InputMouse cl_data_input_mouse_previous;
MenuItem cl_data_menu_items[256];
int cl_data_menu_item_count;
int cl_data_audio_menu_back;
void* cl_data_audio_window;
BuildingEditor cl_data_building_editor;
EditorView cl_data_building_view;
int cl_data_editor_tile_count;
EditorTile cl_data_editor_tiles[64];
vec3 cl_data_edit_vertices[256];
int cl_data_edit_grabbed[256];
vec3 cl_data_edit_offsets[256];
float cl_data_edit_mouse_before[2];
int cl_data_game_block_count;
EditorEntry cl_data_game_blocks[1024];
int cl_data_game_texture_count;
EditorEntry cl_data_game_textures[1024];

#endif
