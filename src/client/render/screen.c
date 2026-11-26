/* The display list stores width, height and pixel depth.
 * Screenshots read the back buffer and write a numbered TGA file.
 * The file contains an 18 byte header followed by BGR pixels.
 */
#include <stdio.h>
#include "render/lists.h"
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

enum { MAX_DISPLAY_MODES = 512, PIXEL_BITS = 32, SHOT_WIDTH = 1280, SHOT_HEIGHT = 960,
       SHOT_LIMIT = 1000, READ_BACK = 0x405, PIXEL_BYTE = 0x1401, TGA_TRUE_COLOR = 2, TGA_BITS = 24 };


/* Append display modes and the fixed 1024 by 640 mode. */
void __cdecl listvideomodes(void)
{
    typedef struct DisplayMode { uint32_t format; int width, height, refresh; void *driver; } DisplayMode;
    DisplayMode mode;
    int index, existing;
    for (index = 0; index < SDL_GetNumDisplayModes(0); ++index) {
        if (index >= MAX_DISPLAY_MODES)
            continue;
        SDL_GetDisplayMode(0, index, &mode);
        for (existing = 0; existing < cl_data_video_mode_count; ++existing) {
            VideoMode *stored = &cl_data_video_modes[existing];
            if (stored->width == mode.width) {
                /* A matching width replaces the height before the match test. */
                stored->height = mode.height;
                if (mode.height)
                    break;
            }
        }
        if (existing == cl_data_video_mode_count) {
            VideoMode *stored = &cl_data_video_modes[cl_data_video_mode_count++];
            stored->width = mode.width;
            stored->height = mode.height;
            stored->bits = PIXEL_BITS;
        }
    }
    cl_data_video_modes[cl_data_video_mode_count].width = 1024;
    cl_data_video_modes[cl_data_video_mode_count].height = 640;
    cl_data_video_modes[cl_data_video_mode_count++].bits = PIXEL_BITS;
}

/* Save the back buffer when its dimensions fit the pixel array. */
void __cdecl screenshot(void)
{
    char filename[16] = "scsht000.tga";
    uint8_t byte;
    uint16_t word;
    int index, x, y;
    if (cl_data_render_width > SHOT_WIDTH || cl_data_render_height > SHOT_HEIGHT)
        return;
    glReadBuffer(READ_BACK);
    glReadPixels(0, 0, cl_data_render_width, cl_data_render_height, GL_RGBA, PIXEL_BYTE, cl_data_screenshot_pixels);
    index = 0;
    for (;;) {
        cl_data_open_file = (int)(uintptr_t)os_fopen(filename, "rb");
        if (!cl_data_open_file || index >= SHOT_LIMIT)
            break;
        ++index;
        filename[5] = (char)('0' + index / 100);
        filename[6] = (char)('0' + (index / 10) % 10);
        filename[7] = (char)('0' + index % 10);
        os_fclose((void *)(uintptr_t)cl_data_open_file);
    }
    cl_data_open_file = (int)(uintptr_t)os_fopen(filename, "wb");
    if (!cl_data_open_file)
        return;
#define WRITE_HEADER(value, object) do { object = (value); os_fwrite(&object, sizeof(object), 1, (void *)(uintptr_t)cl_data_open_file); } while (0)
    WRITE_HEADER(0, byte);
    WRITE_HEADER(0, byte);
    WRITE_HEADER(TGA_TRUE_COLOR, byte);
    WRITE_HEADER(0, word);
    WRITE_HEADER(0, word);
    WRITE_HEADER(0, byte);
    WRITE_HEADER(0, word);
    WRITE_HEADER(0, word);
    WRITE_HEADER((uint16_t)cl_data_render_width, word);
    WRITE_HEADER((uint16_t)cl_data_render_height, word);
    WRITE_HEADER(TGA_BITS, byte);
    WRITE_HEADER(0, byte);
#undef WRITE_HEADER
    for (y = 0; y < cl_data_render_height; ++y) {
        for (x = 0; x < cl_data_render_width; ++x) {
            uint32_t pixel = cl_data_screenshot_pixels[y * cl_data_render_width + x];
            os_fputc((pixel >> 16) & 255, (void *)(uintptr_t)cl_data_open_file);
            os_fputc((pixel >> 8) & 255, (void *)(uintptr_t)cl_data_open_file);
            os_fputc(pixel & 255, (void *)(uintptr_t)cl_data_open_file);
        }
    }
    os_fclose((void *)(uintptr_t)cl_data_open_file);
}
