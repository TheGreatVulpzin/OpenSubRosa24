/* The OpenGL calls and constants that the texture and landscape code uses. */
#ifndef CL_TEXTURES_OPENGL_H
#define CL_TEXTURES_OPENGL_H

#include <stdint.h>

#include "system_libs.h"

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
