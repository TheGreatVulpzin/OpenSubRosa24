/* The places where the client and the server run different code from the same source file.
 *
 * Most of the game code is the same for both programs. A few functions differ: the client
 * predicts the world, draws it and keeps some data in other places. Each difference has one
 * name here. The first part is the server, the second part the client. sr_runtime.h includes
 * this file after the storage of the program. */
#ifndef SR_VARIANT_H
#define SR_VARIANT_H

#if !defined(SR_CLIENT_NATIVE) && !defined(SR_CLIENT_IMAGE)

/* ---- The server ---- */

/* The tick of the game, the end time of the round, the game type and the file of the text parser. */
#define sr_game_ticks sr_game.ticks
#define sr_game_end_time sr_game.end_time
#define sr_option_gametype(o) ((o)->gametype)
#define sr_parser_file sr_data_parser.file
#ifndef sr_voice_channel_type
#define sr_voice_channel_type VoiceChannel
#endif
#define sr_chat_mode 0

/* Parts of the work that only the client does. The server does nothing there. */
#define SR_DEBUG_DISPLAY 0
#define sr_area_reset_interiors() ((void)0)
#define sr_window_atlas(texture, type) 0
#define sr_resetgame_screen() ((void)0)
#define sr_resetgame_level_built() ((void)0)
#define sr_resetgame_light() ((void)0)
#define sr_resetgame_camera_counter() (sr_game.death_camera_ticks = -1)
#define sr_level_after_landscape() ((void)0)
#define sr_level_after_buildings() ((void)0)
#define sr_level_shop_count 5

/* The simulation of the server. */
#define sr_human_vehicle_limit_protection 1
#define sr_human_floor_all_rays 0
#define sr_object_frame_orthogonal 1
#define sr_linear_curve_negate_first 0
#define sr_socket_disable_closes 0

/* The comparisons that decide a collision and a packed direction. */
#define sr_collision_ge(a, b) ((a) >= (b))
#define sr_collision_le(a, b) ((a) <= (b))
#define sr_triangle_edge(value) (value)
#define sr_orientation_nonpositive(value) ((value) <= 0.0)

/* The timers of the server. */
#define sr_timer_list_type TimerList
#define sr_physics_timers 1
#define sr_timer_name(field, name) nametimer(g_timerlist.field, (name))
#define sr_timer_start(field) starttimer(g_timerlist.field)
#define sr_timer_stop(field) stoptimer(g_timerlist.field)

/* The reader of the objects of a state packet, and the update of a human. */
#define sr_unpack_state_objects() unpackobjectpacket()
#define sr_updatehuman_params int human, int player
#define sr_updatehuman_recipient(record, supplied) (supplied)
#define sr_emit_updatehuman(human, player) createevent_updatehuman((human), (player))

/* The vectors and flags of the solver workspace. */
#define BOND_HUMAN_VEL(h, j) (sr_bondhumans[h].bones[j].vel)
#define BOND_HUMAN_DV(h, j) (sr_bondhumans[h].bones[j].dv)
#define BOND_HUMAN_MOMENTUM(h, j) (sr_bondhumans[h].bones[j].ang_momentum)
#define BOND_HUMAN_W(h, j) (sr_bondhumans[h].bones[j].w)
#define BOND_HUMAN_DW(h, j) (sr_bondhumans[h].bones[j].dw)
#define BOND_HUMAN_ACTIVE(h) (sr_bondhumans[h].active != 0)
#define BOND_HUMAN_SOLVES(h, j) (((unsigned)sr_bondhumans[h].mask & (1u << (j))) != 0)
#define BOND_HUMAN_CLEAR(h, j) BOND_HUMAN_SOLVES(h, j)
#define BOND_HUMAN_RESET(h) (sr_bondhumans[h].active = 0)
#define BOND_HUMAN_PREPARE(h) (sr_bondhumans[h].active = 1, sr_bondhumans[h].mask = 0)
#define BOND_HUMAN_MARK(h, j) (sr_bondhumans[h].mask |= 1 << (j))
#define BOND_HUMAN_AXIS(h, j, k) (sr_bondhumans[h].bones[j].axes[k].dir)
#define BOND_HUMAN_INERTIA(h, j, axis) (sr_bondhumans[h].bones[j].axes[axis].k)
#define BOND_HUMAN_COPY_AXIS(h, j, k, value, inertia) \
  (BOND_HUMAN_AXIS(h, j, k) = (value), BOND_HUMAN_INERTIA(h, j, k) = (inertia))
#define BOND_COPY_MODE 1
#define BOND_MODE(b, o) ((uint16_t)(o)->mode)
#define BOND_BALANCE_NONNEGATIVE(value) (0.0 <= (value))
#define BOND_BONE_SPRING 0

#else

/* ---- The client ---- */

/* The OpenGL extension loader of GLee: it fills the pointers of the slots and sets the extension flags. */
int __cdecl GLeeInit(void);
char __cdecl GLeeEnabled(const uint8_t *flag);
/* The text that a place number of the client executable names (image decoder messages). */
#ifdef SR_CLIENT_NATIVE
#include "image/texts.h"
#define CL_TEXT(rva) cl_text(rva)
#define CL_WTEXT(rva) cl_wtext(rva)
#else
#define CL_TEXT(rva) ((const char *)(sr_base + (rva)))
#define CL_WTEXT(rva) ((const wchar_t *)(sr_base + (rva)))
#endif

/* The text parser keeps its file handle in one global, not in a field of Parser. */
#define sr_parser_file (*(void **)&cl_data_open_file)
/* The client record has no end time and no game type: the client keeps them in its own globals. */
extern int sr_client_gametype;
#define sr_option_gametype(o) (*((void)(o), &sr_client_gametype))
extern int sr_client_end_time;
#define sr_game_end_time sr_client_end_time
#define sr_game_ticks sr_game.ticks
#define sr_chat_mode cl_data_audio_voice_mode

/* Parts of the work that only the client does. */
#define SR_DEBUG_DISPLAY 1
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

/* The simulation of the client. */
#define sr_human_floor_all_rays 1
#define sr_human_vehicle_limit_protection 0
#define sr_object_frame_orthogonal 0
#define sr_linear_curve_negate_first 1
#define sr_socket_disable_closes 1

/* The comparisons that decide a collision and a packed direction. */
#define sr_collision_ge(a, b) (!((a) < (b)))
#define sr_collision_le(a, b) (!((a) > (b)))
#define sr_triangle_edge(value) ((float)(value))
#define sr_orientation_nonpositive(value) (!(0.0 < (value)))

/* The client has no timers of its own for these sections. */
#define sr_timer_list_type TimerListClient
#define sr_physics_timers 0
#define sr_timer_name(field, name) ((void)0)
#define sr_timer_start(field) ((void)0)
#define sr_timer_stop(field) ((void)0)

/* The reader of the objects of a state packet, and the update of a human. */
void __cdecl cl_unpackobjectpacket(void);
#define sr_unpack_state_objects() cl_unpackobjectpacket()
#define sr_updatehuman_params int human
#define sr_updatehuman_recipient(record, supplied) ((record)->player_id)
#define sr_emit_updatehuman(human, player) createevent_updatehuman((human))

/* The vectors and flags of the solver workspace of the client. */
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

#endif

#endif
