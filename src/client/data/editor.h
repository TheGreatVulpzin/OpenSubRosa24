/* Declarations of the data of the building, level and model editors. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_EDITOR_H
#define CL_DATA_EDITOR_H
#ifndef SR_STORAGE_GUARDS
extern int cl_data_building_click_wait;
extern InputMouse cl_data_input_mouse_previous;
extern MenuItem cl_data_menu_items[256];
extern int cl_data_menu_item_count;
extern int cl_data_audio_menu_back;
extern void* cl_data_audio_window;
extern BuildingEditor cl_data_building_editor;
extern EditorView cl_data_building_view;
extern int cl_data_editor_tile_count;
extern EditorTile cl_data_editor_tiles[64];
extern vec3 cl_data_edit_vertices[256];
extern int cl_data_edit_grabbed[256];
extern vec3 cl_data_edit_offsets[256];
extern float cl_data_edit_mouse_before[2];
extern int cl_data_game_block_count;
extern EditorEntry cl_data_game_blocks[1024];
extern int cl_data_game_texture_count;
extern EditorEntry cl_data_game_textures[1024];
#endif
#endif
