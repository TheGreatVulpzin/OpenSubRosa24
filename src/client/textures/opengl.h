/* The OpenGL calls and constants that the texture and landscape code uses. */
#ifndef CL_TEXTURES_OPENGL_H
#define CL_TEXTURES_OPENGL_H

#include <stdint.h>

void *sr_import(const char *dll, const char *name);

/* Make a function that calls an OpenGL import. The first call finds the import. */
#define GL_IMPORT(name, params, args)                                \
    static __inline void name params                                 \
    {                                                                \
        static void(__stdcall * call) params;                        \
        if (!call)                                                   \
            *(void **)&call = sr_import("OPENGL32.DLL", #name);      \
        call args;                                                   \
    }

GL_IMPORT(glEnable, (uint32_t cap), (cap))
GL_IMPORT(glDisable, (uint32_t cap), (cap))
GL_IMPORT(glDepthMask, (uint8_t flag), (flag))
GL_IMPORT(glColorMask, (uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha), (red, green, blue, alpha))
GL_IMPORT(glBlendFunc, (uint32_t source, uint32_t dest), (source, dest))
GL_IMPORT(glViewport, (int x, int y, int width, int height), (x, y, width, height))
GL_IMPORT(glMatrixMode, (uint32_t mode), (mode))
GL_IMPORT(glLoadIdentity, (void), ())
GL_IMPORT(glOrtho, (double left, double right, double bottom, double top, double z_near, double z_far),
          (left, right, bottom, top, z_near, z_far))
GL_IMPORT(glBegin, (uint32_t mode), (mode))
GL_IMPORT(glEnd, (void), ())
GL_IMPORT(glColor4f, (float red, float green, float blue, float alpha), (red, green, blue, alpha))
GL_IMPORT(glVertex2f, (float x, float y), (x, y))
GL_IMPORT(glBindTexture, (uint32_t target, uint32_t texture), (target, texture))
GL_IMPORT(glTexParameterf, (uint32_t target, uint32_t name, float value), (target, name, value))
GL_IMPORT(glCopyTexSubImage2D, (uint32_t target, int level, int x_offset, int y_offset, int x, int y, int width, int height),
          (target, level, x_offset, y_offset, x, y, width, height))
GL_IMPORT(glVertexPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glDrawElements, (uint32_t mode, int count, uint32_t type, const void *indices), (mode, count, type, indices))

/* The buffer calls are extensions. The client holds a pointer to each one. */
typedef void(__stdcall *GlBindBufferFn)(uint32_t target, uint32_t buffer);
typedef void(__stdcall *GlBufferDataFn)(uint32_t target, int size, const void *data, uint32_t usage);

#define GL_TRIANGLES 0x0004
#define GL_QUADS 0x0007
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_TEST 0x0b71
#define GL_BLEND 0x0be2
#define GL_TEXTURE_2D 0x0de1
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_TEXTURE_MAX_ANISOTROPY 0x84fe
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88e4

#endif /* CL_TEXTURES_OPENGL_H */
