/* Data of the 3D view: frustum, light, shadow lists, cube tables, graphics state. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

int cl_data_fontnum;
char cl_data_source_buffer[131072];
ModelVertex cl_data_track_verts[917504];
float cl_data_atmo_constant[4] = { 0.050227597f, 0.0001f, 6.4f, 637.1f };
float cl_data_atmo_wavelength[3] = { 0.625f, 0.75f, 1.0f };
int cl_data_sector_block;
Model cl_data_model[1024];
Texture cl_data_texture[4096];
VboRec cl_data_vbo[4096];
LandSectorLod cl_data_landsector_lod[1024];
SphereLevel cl_data_sphere[8];
BoneVert cl_data_cornervert[4096];
char cl_data_draw_text[1024];
char cl_data_draw_number[1024];
int cl_data_glext[17];
ProgramObject cl_data_programs[32];
char cl_data_shader_log[65536];
char cl_data_info_log[65536];
int cl_data_render_width;
int cl_data_render_height;
int cl_data_open_file;
float cl_data_light_position[4];
vec3 cl_data_world_position;
float cl_data_viewport_width;
float cl_data_viewport_height;
int cl_data_track_vbo;
int cl_data_track_vertex_floats;
int cl_data_track_vertex_count;
vec3 cl_data_frustum_planes[5] = { { 0.0f, 0.0f, 1.0f }, { -0.7071f, 0.0f, 0.7071f }, { 0.7071f, 0.0f, 0.7071f }, { 0.0f, -0.7071f, 0.7071f }, { 0.0f, 0.7071f, 0.7071f } };
vec3 cl_data_shadow_corners[6];
int cl_data_shadow_count;
/* Room for 1048576 points. The shadow volume of the interiors of one sector has 24 points for each triangle
 * and a sector holds 4 interiors, so the list needs more than 16384 points. */
vec3 cl_data_shadow_list[1048576];
uint32_t cl_data_chat_head;
SpriteModel cl_data_sprite_models[16384];
SpriteVertex cl_data_shell_verts[262144];
SpriteVertex cl_data_debris_verts[65536];
int cl_data_score_count;
ScoreEntry cl_data_score_entries[256];
char cl_data_display_text[256];
float cl_data_light_ambient[4];
vec3 cl_data_interior_corners[6];
int cl_data_sector_part_count[128];
int cl_data_sector_draw_counts[1048576];
void* cl_data_sector_draw_offsets[1048576];
int cl_data_sector_count;
int cl_data_sector_list[1024];
SectorEntry cl_data_sector_entries[1024];
int cl_data_interior_model_count;
uint8_t cl_data_sector_vertices[33554432];
uint8_t cl_data_sector_indices[6291456];
int cl_data_sector_vbo;
ClientSprite cl_data_sprites[16384];
SpriteDecalVertex cl_data_sprite_decals[16384];
GlassVertex cl_data_glass_vertices[16384];
Tiremark cl_data_tiremarks[16384];
int cl_data_selected_item;
vec3 cl_data_sector_shadow_corners[6];
int cl_data_sector_shadow_count;
vec3 cl_data_sector_shadow_vertices[262144];
int cl_data_sector_shadow_buffer_ready;
int cl_data_frame_model_count;
int cl_data_frame_model_order[4096];
FrameEntry cl_data_frame_models[4096];
uint32_t cl_data_screenshot_pixels[1228800];
float cl_data_blur_params_near[8];
float cl_data_viewport_x;
float cl_data_viewport_y;
float cl_data_light_diffuse[4];
float cl_data_sky_color[3];
float cl_data_view_near;
float cl_data_view_far;
float cl_data_view_fov;
vec3 cl_data_view_side_right;
vec3 cl_data_view_side_left;
int cl_data_window_quad_count;
WindowQuad cl_data_window_quads[16384];
ClientFramebuffer cl_data_framebuffers[2];
int cl_data_framebuffer_index;
uint32_t cl_data_input_debug_toggle;
AudioCamera cl_data_audio_camera;
int cl_data_audio_input_blocked;

#endif
