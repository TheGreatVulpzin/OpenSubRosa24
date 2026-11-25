/* The OpenGL extensions of the client. GLee loads them. The game keeps its own flags in glext:
 * a flag is 1 when GLee found the extension. */
#include "render/glext.h"

#include "client.h"
#include "sr_runtime.h"

/* Game functions of other files (GLee). The client links GLee, so it keeps its own flags and
 * slots: GLeeInit fills them, and GLeeEnabled reads a flag. */
#define GLeeInit CL_FN_NAMED(GLeeInit, 0x430740, int(__cdecl *)(void))
#define GLeeEnabled CL_FN_NAMED(GLeeEnabled, 0x430720, char(__cdecl *)(const uint8_t *flag))

enum {
    GLEXT_MULTITEXTURE = 0,
    GLEXT_SHADERS = 1, /* 1, 2 and 3 come from the one flag of shader objects */
    GLEXT_BUFFERS = 6,
    GLEXT_STENCIL_TWO_SIDE = 8,
    GLEXT_SEPARATE_STENCIL = 9,
    GLEXT_MULTI_DRAW = 12,
    GLEXT_FRAMEBUFFERS = 16
};

/* Load the OpenGL extensions through GLee and set the extension flags of the game. A flag that
 * GLee does not set keeps its old value. */
void __cdecl setupglextentions(void)
{
    GLeeInit();
    if (GLeeEnabled(&cl_data_glee_arb_multitexture) != 0)
        cl_data_glext[GLEXT_MULTITEXTURE] = 1;
    if (GLeeEnabled(&cl_data_glee_arb_shader_objects) != 0) {
        cl_data_glext[GLEXT_SHADERS] = 1;
        cl_data_glext[GLEXT_SHADERS + 1] = 1;
        cl_data_glext[GLEXT_SHADERS + 2] = 1;
    }
    if (GLeeEnabled(&cl_data_glee_arb_vertex_buffer_object) != 0)
        cl_data_glext[GLEXT_BUFFERS] = 1;
    if (GLeeEnabled(&cl_data_glee_ext_stencil_two_side) != 0)
        cl_data_glext[GLEXT_STENCIL_TWO_SIDE] = 1;
    if (GLeeEnabled(&cl_data_glee_ati_separate_stencil) != 0)
        cl_data_glext[GLEXT_SEPARATE_STENCIL] = 1;
    if (GLeeEnabled(&cl_data_glee_arb_framebuffer_object) != 0)
        cl_data_glext[GLEXT_FRAMEBUFFERS] = 1;
    if (GLeeEnabled(&cl_data_glee_ext_multi_draw_arrays) != 0)
        cl_data_glext[GLEXT_MULTI_DRAW] = 1;
}
