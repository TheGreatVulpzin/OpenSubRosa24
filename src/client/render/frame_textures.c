/* The textures that the frame passes draw into, at the render size, and the frame buffer that
 * holds the color texture and a depth and stencil buffer. */
#include "render/frame_textures.h"

#include "client.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/upload.h"

enum { GL_RGBA = 0x1908, GL_RGBA8 = 0x8058, GL_CLAMP_TO_EDGE = 0x812f, GL_NEAREST = 0x2600,
       GL_LINEAR = 0x2601, GL_TEXTURE_2D = 0xde1, FRAMEBUFFER = 0x8d40, RENDERBUFFER = 0x8d41,
       COLOR_ATTACHMENT = 0x8ce0, DEPTH_STENCIL_ATTACHMENT = 0x821a, DEPTH24_STENCIL8 = 0x88f0,
       EXT_FRAMEBUFFER = 16, BLUR_TAPS = 8,
       SLOT_BIND_RENDERBUFFER = 0x115bb0, SLOT_RENDERBUFFER_STORAGE = 0x115bbc,
       SLOT_BIND_FRAMEBUFFER = 0x115bc8, SLOT_CHECK_FRAMEBUFFER = 0x115bd4,
       SLOT_ATTACH_TEXTURE = 0x115bdc, SLOT_ATTACH_RENDERBUFFER = 0x115be4 };

typedef void(__stdcall *BindFn)(uint32_t target, uint32_t name);
typedef void(__stdcall *StorageFn)(uint32_t target, uint32_t format, int width, int height);
typedef uint32_t(__stdcall *CheckFn)(uint32_t target);
typedef void(__stdcall *AttachTextureFn)(uint32_t, uint32_t, uint32_t, uint32_t, int);
typedef void(__stdcall *AttachRenderbufferFn)(uint32_t, uint32_t, uint32_t, uint32_t);
#define gl_slot(type, slot) (*(type *)(sr_base + (slot)))

/* Make a frame texture of the render size with no pixels in memory. */
static void frame_texture(int index, int filter)
{
    Texture *texture = &cl_data_texture[index];

    texture->width = cl_data_render_width;
    texture->height = cl_data_render_height;
    texture->num_levels = 1;
    texture->format = GL_RGBA;
    texture->internal_format = GL_RGBA8;
    texture->wrap_s = GL_CLAMP_TO_EDGE;
    texture->wrap_t = GL_CLAMP_TO_EDGE;
    texture->mag_filter = filter;
    texture->min_filter = filter;
    os_free(texture->levels[0]);
    texture->levels[0] = 0;
    setuptexture(index);
}

/* The weights of the blur: cos(i * pi / 17) for i = 0 to 7, divided by 0.875 times the sum of
 * the 15 taps (i = -7 to 7). Each sum step rounds to a float. The eight weights fill
 * blur_params_near and blur_params_far. */
static void blur_weights(void)
{
    float *weights = cl_data_blur_params_near;
    float sum = 0.0f;
    int i;

    for (i = 0; i < BLUR_TAPS; ++i)
        sum = (float)(x87_cos((double)i * 3.14159265359 / 17.0) + sum);
    for (i = 1; i < BLUR_TAPS; ++i)
        sum = (float)(x87_cos((double)i * 3.14159265359 / 17.0) + sum);
    for (i = 0; i < BLUR_TAPS; ++i)
        weights[i] = (float)(x87_cos((double)i * 3.14159265359 / 17.0) / (float)(sum * 0.875));
}

/* Make the frame textures at the render size, and attach the color texture and a depth and
 * stencil buffer to the frame buffer when the card has frame buffers. The sky texture uses no
 * filter. */
void __cdecl setupframetextures(void)
{
    const ClientFramebuffer *target = &cl_data_framebuffers[cl_data_framebuffer_index];

    blur_weights();
    frame_texture(sr_data_texturelist.frame_color, GL_LINEAR);
    frame_texture(sr_data_texturelist.shadow_map, GL_LINEAR);
    frame_texture(sr_data_texturelist.sky, GL_NEAREST);
    frame_texture(sr_data_texturelist.frame_aux, GL_LINEAR);
    frame_texture(sr_data_texturelist.frame_spare, GL_LINEAR);
    if (cl_data_glext[EXT_FRAMEBUFFER] == 0)
        return;
    gl_slot(BindFn, SLOT_BIND_FRAMEBUFFER)(FRAMEBUFFER, target->framebuffer);
    gl_slot(AttachTextureFn, SLOT_ATTACH_TEXTURE)(FRAMEBUFFER, COLOR_ATTACHMENT, GL_TEXTURE_2D,
        cl_data_texture[sr_data_texturelist.frame_color].gl_name, 0);
    gl_slot(BindFn, SLOT_BIND_RENDERBUFFER)(RENDERBUFFER, target->renderbuffer);
    gl_slot(StorageFn, SLOT_RENDERBUFFER_STORAGE)(RENDERBUFFER, DEPTH24_STENCIL8, cl_data_render_width,
        cl_data_render_height);
    gl_slot(AttachRenderbufferFn, SLOT_ATTACH_RENDERBUFFER)(FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, RENDERBUFFER,
        target->renderbuffer);
    gl_slot(CheckFn, SLOT_CHECK_FRAMEBUFFER)(FRAMEBUFFER);
    gl_slot(BindFn, SLOT_BIND_FRAMEBUFFER)(FRAMEBUFFER, 0);
}
