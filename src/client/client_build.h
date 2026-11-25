/* Hand-written macros and prototypes that only the client build uses. The generated
   storage adapter includes this file after the data names, so a macro here may use any
   cl_data_ or sr_data_ name. Add new client hook macros here, not in the generator. */
#ifndef CLIENT_BUILD_H
#define CLIENT_BUILD_H

/* The text parser keeps its file handle in one global, not in a field of Parser. */
#define sr_parser_file (*(void **)&cl_data_open_file)
/* The client keeps the tick before the place of Game.ticks. Shared code reads a signed int. */
#define sr_game_ticks (*(int *)&cl_data_game_tick)
void __cdecl debug_addcross(const vec3 *center, float size);
#define sr_debug_cross(center, size) debug_addcross((center), (size))
void __cdecl debug_addatafloat(const char *name, float value);
#define sr_debug_float(name, value) debug_addatafloat((name), (value))
void __cdecl debug_addline(const vec3 *from, const vec3 *to);
#define sr_debug_line(from, to) debug_addline((from), (to))
void area_reset_interiors(void);
#define sr_area_reset_interiors() area_reset_interiors()
int __cdecl addwindowatlas(int texture, int type);
#define sr_window_atlas(texture, type) addwindowatlas((texture), (type))
#define sr_object_frame_orthogonal 0
void level_client_landscape(void);
#define sr_level_after_landscape() level_client_landscape()
void level_client_finish(void);
#define sr_level_after_buildings() level_client_finish()
#define sr_level_shop_count 4
void resetgame_client_screen(void);
#define sr_resetgame_screen() resetgame_client_screen()
void resetgame_client_level_built(void);
#define sr_resetgame_level_built() resetgame_client_level_built()
void resetgame_client_light(void);
#define sr_resetgame_light() resetgame_client_light()
void resetgame_client_camera_counter(void);
#define sr_resetgame_camera_counter() resetgame_client_camera_counter()
void __cdecl cl_unpackobjectpacket(void);
#define sr_unpack_state_objects() cl_unpackobjectpacket()
#define sr_human_floor_all_rays 1
#define sr_human_vehicle_limit_protection 0
#define BOND_HUMAN_VEL(h, j) (cl_data_bondhuman_client[h].vel[j].value)
#define BOND_HUMAN_DV(h, j) (cl_data_bondhuman_client[h].dv[j].value)
#define BOND_HUMAN_MOMENTUM(h, j) (cl_data_bondhuman_client[h].ang_momentum[j].value)
#define BOND_HUMAN_W(h, j) (cl_data_bondhuman_client[h].w[j].value)
#define BOND_HUMAN_DW(h, j) (cl_data_bondhuman_client[h].dw[j].value)
#define BOND_HUMAN_ACTIVE(h) (sr_data_human[h].active != 0 && sr_data_human[h].has_physics != 0)
#define BOND_HUMAN_SOLVES(h, j) (0.0 < sr_data_human[h].bones[j].in_solver)
#define BOND_HUMAN_CLEAR(h, j) 1
#define BOND_HUMAN_RESET(h) ((void)0)
#define BOND_HUMAN_PREPARE(h) ((void)0)
#define BOND_HUMAN_MARK(h, j) ((void)0)
#define BOND_HUMAN_AXIS(h, j, k) (sr_data_human[h].bones[j].rot[k])
#define BOND_HUMAN_INERTIA(h, j, k) (((const float *)&sr_data_human[h].bones[j].inv_inertia)[k])
#define BOND_HUMAN_COPY_AXIS(h, j, k, value, inertia) ((void)0)
#define BOND_COPY_MODE 0
#define BOND_MODE(b, o) ((unsigned)(b)->mode)
#define BOND_BALANCE_NONNEGATIVE(value) (!((value) < 0.0))
#define BOND_BONE_SPRING 1
#define sr_updatehuman_params int human
#define sr_updatehuman_recipient(record, supplied) ((record)->player_id)
#define sr_emit_updatehuman(human, player) createevent_updatehuman((human))
#define sr_chat_mode cl_data_audio_voice_mode
#define sr_timer_list_type TimerListClient
#define sr_physics_timers 0
#define sr_socket_disable_closes 1
#define sr_triangle_edge(value) ((float)(value))
#define sr_orientation_nonpositive(value) (!(0.0 < (value)))
#define sr_collision_ge(a, b) (!((a) < (b)))
#define sr_collision_le(a, b) (!((a) > (b)))
#define sr_linear_curve_negate_first 1
#define sr_timer_name(field, name) ((void)0)
#define sr_timer_start(field) ((void)0)
#define sr_timer_stop(field) ((void)0)
extern int sr_client_gametype;
#define sr_option_gametype(o) (*((void)(o), &sr_client_gametype))
extern int sr_client_end_time;
#define sr_game_end_time sr_client_end_time

#endif
