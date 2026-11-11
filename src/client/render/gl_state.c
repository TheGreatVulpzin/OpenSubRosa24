/* The OpenGL states that the render passes switch between.
 *
 * glext holds one flag for each extension that the client checks at start. Entry 8 is the
 * two sided stencil of EXT and entry 9 is the separate stencil operation of ATI. A pass
 * writes the stencil of the front and back faces in one go when one of them is there.
 */
#include "render/gl_state.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_glext cl_data_glext

enum {
    GLEXT_STENCIL_TWO_SIDE = 8,
    GLEXT_STENCIL_SEPARATE = 9,
    GL_STENCIL_TEST = 0x0b90,
    GL_CULL_FACE = 0x0b44,
    GL_STENCIL_TEST_TWO_SIDE = 0x8910,
    GL_FRONT = 0x0404,
    GL_BACK = 0x0405,
    GL_CW = 0x0900,
    GL_KEEP = 0x1e00,
    GL_ALWAYS = 0x0207,
    GL_LEQUAL = 0x0203,
    GL_GEQUAL = 0x0206,
    GL_INCR_WRAP = 0x8507,
    GL_DECR_WRAP = 0x8508
};

/* The two functions of the stencil extensions: the client holds a pointer to each one. */
#define SLOT_ACTIVE_STENCIL_FACE 0x116450
#define SLOT_STENCIL_OP_SEPARATE 0x116590

typedef void(__stdcall *ActiveStencilFaceFn)(uint32_t face);
typedef void(__stdcall *StencilOpSeparateFn)(uint32_t face, uint32_t fail, uint32_t depth_fail, uint32_t pass);

GL_IMPORT(glFrontFace, (uint32_t mode), (mode))
GL_IMPORT(glStencilMask, (uint32_t mask), (mask))
GL_IMPORT(glStencilFunc, (uint32_t function, int reference, uint32_t mask), (function, reference, mask))
GL_IMPORT(glStencilOp, (uint32_t fail, uint32_t depth_fail, uint32_t pass), (fail, depth_fail, pass))
GL_IMPORT(glDepthFunc, (uint32_t function), (function))

/* Turn on the two sided stencil test when the extension is there. */
void __cdecl enabletwosidedstencil(void)
{
    if (g_glext[GLEXT_STENCIL_TWO_SIDE] != 0)
        glEnable(GL_STENCIL_TEST_TWO_SIDE);
}

/* Turn off the two sided stencil test when the extension is there. */
void __cdecl disabletwosidedstencil(void)
{
    if (g_glext[GLEXT_STENCIL_TWO_SIDE] != 0)
        glDisable(GL_STENCIL_TEST_TWO_SIDE);
}

/* The state of the shadow pass: no color, no depth write, and the stencil counts up on the
 * back faces and down on the front faces. */
void __cdecl glstateshadowtwoside(void)
{
    ActiveStencilFaceFn active_face = *(ActiveStencilFaceFn *)(sr_base + SLOT_ACTIVE_STENCIL_FACE);
    StencilOpSeparateFn stencil_op = *(StencilOpSeparateFn *)(sr_base + SLOT_STENCIL_OP_SEPARATE);

    glDisable(GL_TEXTURE_2D);
    glColorMask(0, 0, 0, 0);
    glDepthMask(0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GEQUAL);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    if (g_glext[GLEXT_STENCIL_TWO_SIDE] == 0) {
        if (g_glext[GLEXT_STENCIL_SEPARATE] != 0) {
            glEnable(GL_STENCIL_TEST);
            glStencilMask(0xffffffffu);
            glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
            stencil_op(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
            stencil_op(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
        }
    } else {
        glEnable(GL_STENCIL_TEST);
        glEnable(GL_STENCIL_TEST_TWO_SIDE);
        active_face(GL_BACK);
        glStencilMask(0xffffffffu);
        glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
        glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP);
        active_face(GL_FRONT);
        glStencilMask(0xffffffffu);
        glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    }
}

/* The state of the normal pass: colors on, depth write on, stencil off. */
void __cdecl glstatenormal(void)
{
    glEnable(GL_TEXTURE_2D);
    glColorMask(1, 1, 1, 0);
    glDepthMask(1);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glDisable(GL_STENCIL_TEST);
}
