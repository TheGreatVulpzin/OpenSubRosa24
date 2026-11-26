/* The data of the client. One file for each system in src/client/data. */
#ifndef SR_CLIENT_STORAGE_H
#define SR_CLIENT_STORAGE_H
#include "sr_storage_types.h"
#define sr_voice_channel_type VoiceChannelClient
#include "sr_image_types.h"
#include "sr_audio_types.h"
#include "sr_input_types.h"
#include "gl_slots.h"

#define cl_data_gl_buffer_sub_data cl_glBufferSubDataARB
#define cl_data_gl_bind_buffer cl_glBindBufferARB
#define cl_data_gl_buffer_data cl_glBufferDataARB
#define cl_data_gl_create_program cl_glCreateProgramObjectARB
#define cl_data_gl_create_shader cl_glCreateShaderObjectARB
#define cl_data_gl_gen_buffers cl_glGenBuffersARB
#define cl_data_gl_gen_framebuffers cl_glGenFramebuffers
#define cl_data_gl_gen_renderbuffers cl_glGenRenderbuffers
extern uint8_t _GLEE_ARB_multitexture;
#define cl_data_glee_arb_multitexture _GLEE_ARB_multitexture
extern uint8_t _GLEE_ARB_vertex_buffer_object;
#define cl_data_glee_arb_vertex_buffer_object _GLEE_ARB_vertex_buffer_object
extern uint8_t _GLEE_ARB_shader_objects;
#define cl_data_glee_arb_shader_objects _GLEE_ARB_shader_objects
extern uint8_t _GLEE_ARB_framebuffer_object;
#define cl_data_glee_arb_framebuffer_object _GLEE_ARB_framebuffer_object
extern uint8_t _GLEE_EXT_multi_draw_arrays;
#define cl_data_glee_ext_multi_draw_arrays _GLEE_EXT_multi_draw_arrays
extern uint8_t _GLEE_EXT_stencil_two_side;
#define cl_data_glee_ext_stencil_two_side _GLEE_EXT_stencil_two_side
extern uint8_t _GLEE_ATI_separate_stencil;
#define cl_data_glee_ati_separate_stencil _GLEE_ATI_separate_stencil

#define sr_data_view (cl_data_audio_camera.player)


/* The data of each system is in src/client/data. The guard build takes its data from build/guards
 * (see instrumentation/client/gen_guards.py). */
#ifdef SR_STORAGE_GUARDS
#include "guard_data.h"
#else
#include "../client/data/audio.h"
#include "../client/data/editor.h"
#include "../client/data/game.h"
#include "../client/data/image.h"
#include "../client/data/input.h"
#include "../client/data/menu.h"
#include "../client/data/render.h"
#include "../client/data/setup.h"
#include "../client/data/textures.h"
#endif
#include "../client/data/unplaced.h"

#endif