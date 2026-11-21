/* Record calls before the graphics driver receives them. Pointer entries include
 * the current buffer binding, so a client address and a buffer offset differ.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_trace.h"
#include "runtime/gl_slots.h"

enum { TRACE_CALLS = 64, TRACE_TEXT = 512, ARRAY_BINDING = 0x8894,
       ELEMENT_BINDING = 0x8895 };
static char calls[TRACE_CALLS][TRACE_TEXT];
static unsigned next_call;
static char array_state[TRACE_TEXT / 2];

static int enabled(void)
{
    const char *text = getenv("SR_TRACE");
    return text && atoi(text) >= 4;
}

static void record(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(calls[next_call++ % TRACE_CALLS], TRACE_TEXT, format, args);
    va_end(args);
}

static int binding(unsigned name)
{
    int value = 0;
    glGetIntegerv(name, &value);
    return value;
}

static void draw_state(void)
{
    static void (__stdcall *get_buffer)(unsigned, unsigned, int *);
    int size = -1;
    void *vertex = NULL, *normal = NULL, *texture = NULL, *color = NULL;
    if (!get_buffer)
        get_buffer = (void (__stdcall *)(unsigned, unsigned, int *))wglGetProcAddress("glGetBufferParameterivARB");
    if (get_buffer && binding(ARRAY_BINDING)) get_buffer(0x8892, 0x8764, &size);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &vertex);
    glGetPointerv(GL_NORMAL_ARRAY_POINTER, &normal);
    glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &texture);
    glGetPointerv(GL_COLOR_ARRAY_POINTER, &color);
    snprintf(array_state, sizeof array_state, "arrays size=%d vertex=%d/%d/%p normal=%d/%d/%p texture=%d/%d/%p color=%d/%d/%p",
           size, glIsEnabled(GL_VERTEX_ARRAY), binding(0x8896), vertex,
           glIsEnabled(GL_NORMAL_ARRAY), binding(0x8897), normal,
           glIsEnabled(GL_TEXTURE_COORD_ARRAY), binding(0x889a), texture,
           glIsEnabled(GL_COLOR_ARRAY), binding(0x8898), color);
}

#define POINTER_CALL(name) \
static void (__stdcall *real_##name)(int, unsigned, int, const void *); \
static void __stdcall trace_##name(int size, unsigned type, int stride, const void *pointer) \
{ \
    client_gl_trace_slots(); \
    record(#name " size=%d type=%x stride=%d pointer=%p array=%d", \
           size, type, stride, pointer, binding(ARRAY_BINDING)); \
    real_##name(size, type, stride, pointer); \
}
POINTER_CALL(glVertexPointer)
POINTER_CALL(glTexCoordPointer)
POINTER_CALL(glColorPointer)

static void (__stdcall *real_glNormalPointer)(unsigned, int, const void *);
static void __stdcall trace_glNormalPointer(unsigned type, int stride, const void *pointer)
{
    record("glNormalPointer type=%x stride=%d pointer=%p array=%d", type, stride, pointer, binding(ARRAY_BINDING));
    real_glNormalPointer(type, stride, pointer);
}

static void (__stdcall *real_glDrawElements)(unsigned, int, unsigned, const void *);
static void __stdcall trace_glDrawElements(unsigned mode, int count, unsigned type, const void *indices)
{
    draw_state();
    record("glDrawElements mode=%x count=%d type=%x indices=%p array=%d element=%d %s",
           mode, count, type, indices, binding(ARRAY_BINDING), binding(ELEMENT_BINDING), array_state);
    real_glDrawElements(mode, count, type, indices);
}

static void (__stdcall *real_glDrawArrays)(unsigned, int, int);
static void __stdcall trace_glDrawArrays(unsigned mode, int first, int count)
{
    draw_state();
    record("glDrawArrays mode=%x first=%d count=%d array=%d %s", mode, first, count, binding(ARRAY_BINDING), array_state);
    real_glDrawArrays(mode, first, count);
}

static cl_glBindBufferARB_fn real_glBindBufferARB;
static void __stdcall trace_glBindBufferARB(unsigned target, unsigned buffer)
{
    record("glBindBufferARB target=%x buffer=%u", target, buffer);
    real_glBindBufferARB(target, buffer);
}

static cl_glBufferDataARB_fn real_glBufferDataARB;
static void __stdcall trace_glBufferDataARB(unsigned target, int size, const void *data, unsigned usage)
{
    record("glBufferDataARB target=%x size=%d data=%p usage=%x array=%d",
           target, size, data, usage, binding(ARRAY_BINDING));
    real_glBufferDataARB(target, size, data, usage);
}

static cl_glGenBuffersARB_fn real_glGenBuffersARB;
static void __stdcall trace_glGenBuffersARB(int count, unsigned *buffers)
{
    record("glGenBuffersARB count=%d buffers=%p", count, buffers);
    real_glGenBuffersARB(count, buffers);
}

static cl_glBufferSubDataARB_fn real_glBufferSubDataARB;
static void __stdcall trace_glBufferSubDataARB(unsigned target, int offset, int size, const void *data)
{
    record("glBufferSubDataARB target=%x offset=%d size=%d data=%p array=%d",
           target, offset, size, data, binding(ARRAY_BINDING));
    real_glBufferSubDataARB(target, offset, size, data);
}

static cl_glMultiDrawElementsEXT_fn real_glMultiDrawElementsEXT;
static void __stdcall trace_glMultiDrawElementsEXT(unsigned mode, const int *counts, unsigned type,
                                                 const void **indices, int count)
{
    draw_state();
    record("glMultiDrawElementsEXT mode=%x counts=%p first=%d type=%x indices=%p first=%p count=%d array=%d element=%d %s",
           mode, counts, count > 0 ? counts[0] : 0, type, indices, count > 0 ? indices[0] : NULL,
           count, binding(ARRAY_BINDING), binding(ELEMENT_BINDING), array_state);
    real_glMultiDrawElementsEXT(mode, counts, type, indices, count);
}

void client_gl_trace_slots(void)
{
    if (!enabled()) return;
#define SLOT(name) \
    if (cl_##name && cl_##name != trace_##name) { \
        real_##name = cl_##name; \
        cl_##name = trace_##name; \
    }
    SLOT(glBindBufferARB)
    SLOT(glBufferDataARB)
    SLOT(glGenBuffersARB)
    SLOT(glBufferSubDataARB)
    SLOT(glMultiDrawElementsEXT)
#undef SLOT
}

void *client_gl_trace_import(const char *dll, const char *name, void *function)
{
    if (!enabled() || _stricmp(dll, "OPENGL32.DLL")) return function;
#define IMPORT(name_) \
    if (!strcmp(name, #name_)) { \
        *(void **)&real_##name_ = function; \
        return (void *)trace_##name_; \
    }
    IMPORT(glVertexPointer)
    IMPORT(glNormalPointer)
    IMPORT(glTexCoordPointer)
    IMPORT(glColorPointer)
    IMPORT(glDrawElements)
    IMPORT(glDrawArrays)
#undef IMPORT
    return function;
}

void client_gl_trace_dump(void)
{
    unsigned i, first = next_call > TRACE_CALLS ? next_call - TRACE_CALLS : 0;
    if (!enabled()) return;
    fprintf(stderr, "last GL calls, oldest first:\n");
    for (i = first; i < next_call; ++i)
        fprintf(stderr, "  %s\n", calls[i % TRACE_CALLS]);
}

