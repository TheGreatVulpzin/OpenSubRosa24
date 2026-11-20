/* The drawing of the menu items: the round button texture, the two button styles, the text
 * fields and the item list. A text field that the player edits keeps its text in the text
 * input: the selection, the cursor and the text. */
#include "menu/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "menu/items.h"
#include "menu/textinput.h"
#include "render/draw2d.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "textures/upload.h"
#include "vectors.h"

#define MENU_TEXTURE 0x3de
#define MENU_TEXTURE_SIZE 0x20

enum { KIND_TEXT = 1, KIND_TEXT_2 = 2, KIND_TOGGLE = 3, KIND_SLIDER = 5 };

#define BEVEL 2.0
#define CORNER 2.0
#define LIGHT_Z 0.83f
#define GLYPH_CURSOR '_'
#define DRAW_LIMIT 1000
#define TEXT_KEEP 0x100
#define TEXT_SHADOW 0x40
#define TEXT_LABEL 0x60
#define SLIDER_KNOB 8.0
#define TOGGLE_BOX 16.0

GL_IMPORT(glVertex2fv, (const float *vertex), (vertex))
GL_IMPORT(glTexCoord2fv, (const float *coord), (coord))

/* Game functions of other files. */
#define getinputletter CL_FN(0x5046c0, int(__cdecl *)(int wait))
#define drawtext CL_FN(0x506b30, void(__cdecl *)(const char *text, float x, float y, float size, uint32_t flags, \
                                                 float red, float green, float blue, float alpha, ...))


/* Make the texture of the round button corners: a quarter of a white disc. The distance is from
 * the corner of the texture. Pixels inside radius 24 are faint, the ring up to radius 32 is
 * opaque and the pixels outside are clear. */
void __cdecl generatemenutextures(void)
{
    Texture *texture = &cl_data_texture[MENU_TEXTURE];
    int x, y;

    texture->width = MENU_TEXTURE_SIZE;
    texture->height = MENU_TEXTURE_SIZE;
    texture->num_levels = 1;
    texture->format = GL_RGBA;
    texture->internal_format = GL_RGBA;
    texture->spare = 0;
    texture->wrap_s = GL_CLAMP_TO_EDGE;
    texture->wrap_t = GL_CLAMP_TO_EDGE;
    texture->mag_filter = GL_LINEAR;
    texture->min_filter = GL_LINEAR;
    os_free(texture->levels[0]);
    texture->levels[0] = os_malloc((size_t)(texture->width * texture->height * 4));
    for (y = 0; y < texture->height; y++) {
        for (x = 0; x < texture->width; x++) {
            float v[2];
            float length;
            uint32_t alpha = 0x4c;

            v[0] = (float)x;
            v[1] = (float)y;
            length = (float)vectorlength2d(v);
            if (length > 24.0)
                alpha = 0xff;
            if (length > 32.0)
                alpha = 0;
            texture->levels[0][y * texture->width + x] = alpha * 0x1000000u + 0xffffffu;
        }
    }
    setuptexture(MENU_TEXTURE);
}

/* Draw a flat button with a bevel of 2 units. fade tilts the light: each edge of the bevel
 * gets its own shade, and the face gets the shade of a light from above. The inner edge of the
 * bevel has half the alpha. */
void __cdecl drawmenubutton(float x, float y, float width, float height, float fade, uint32_t flags, float red,
                            float green, float blue, float alpha)
{
    /* The weight of the tilt for each edge: top, right, bottom, left. The right and the bottom
     * edge use the negated tilt: the sign change comes before the product, so a NaN tilt
     * changes its sign there. */
    static const float tilt[4] = {0.5f, 0.25f, 0.5f, 0.25f};
    float(*outer)[2] = (float(*)[2])cl_data_menu_vertices;
    float(*inner)[2] = outer + 4;
    float shade;
    int i;

    align_shape(&x, &y, width, height, flags);
    for (i = 0; i < 4; i++) {
        if (i == 0 || i == 3) {
            outer[i][0] = x;
            inner[i][0] = (float)(x + BEVEL);
        } else {
            outer[i][0] = (float)(x + width);
            inner[i][0] = (float)((double)x + width - BEVEL);
        }
        if (i < 2) {
            outer[i][1] = y;
            inner[i][1] = (float)(y + BEVEL);
        } else {
            outer[i][1] = (float)(y + height);
            inner[i][1] = (float)((double)y + height - BEVEL);
        }
    }
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (i = 0; i < 4; i++) {
        float c = (float)(1.0 / sqrt((double)fade * fade + 1.0));
        float s = (float)(fade * c);

        shade = (float)((i == 1 || i == 2 ? -s : s) * tilt[i] + c * LIGHT_Z);
        shade = (float)(shade * 0.5);
        glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), alpha);
        glVertex2fv(outer[i]);
        glVertex2fv(outer[(i + 1) & 3]);
        glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), (float)(alpha * 0.5));
        glVertex2fv(inner[(i + 1) & 3]);
        glVertex2fv(inner[i]);
    }
    shade = (float)(LIGHT_Z * 1.0);
    shade = (float)((fade * 0.125 + 0.375) * shade);
    glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), alpha);
    glVertex2fv(inner[0]);
    glVertex2fv(inner[1]);
    shade = (float)(LIGHT_Z * 1.0);
    shade = (float)((0.375 - fade * 0.125 * 0.5) * shade);
    glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), (float)(alpha * 0.5));
    glVertex2fv(inner[2]);
    glVertex2fv(inner[3]);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

/* The texture corners of the 16 vertices: each corner of the button gets the quarter disc of
 * the round texture, turned to face out. */
static const float corner_uv[16][2] = {
    {1, 1}, {0, 1}, {0, 0}, {1, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0},
    {0, 0}, {1, 0}, {1, 1}, {0, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1},
};

/* The quads between the corner squares: top, right, bottom, left and the middle. */
static const int edge_quads[5][4] = {{1, 4, 7, 2}, {7, 6, 9, 8}, {13, 8, 11, 14}, {3, 2, 13, 12}, {2, 7, 8, 13}};

/* Draw a button with round corners: four squares of 2 units with the round texture, and the
 * quads between them. The alpha goes from 1 at the top to 0.25 at the bottom. A button that is
 * not lit has half the alpha. */
void __cdecl drawmenubutton2(float x, float y, float width, float height, int lit, uint32_t flags, float red,
                             float green, float blue, float alpha)
{
    float(*vertex)[2] = (float(*)[2])cl_data_menu_vertices;
    float(*texcoord)[2] = (float(*)[2])cl_data_menu_texcoords;
    int(*quads)[4] = (int(*)[4])cl_data_menu_quads;
    int corner, k;

    align_shape(&x, &y, width, height, flags);
    for (corner = 0; corner < 4; corner++) {
        float left = corner == 0 || corner == 3 ? x : (float)((double)x + width - CORNER);
        float top = corner < 2 ? y : (float)((double)y + height - CORNER);

        for (k = 0; k < 4; k++) {
            int v = corner * 4 + k;

            vertex[v][0] = k == 0 || k == 3 ? left : (float)(left + CORNER);
            vertex[v][1] = k < 2 ? top : (float)(top + CORNER);
            cl_data_menu_alpha[v] = (float)((1.0 - (vertex[v][1] - y) / height) * 0.75 + 0.25);
        }
        memcpy(texcoord[corner * 4], corner_uv[corner * 4], sizeof corner_uv[0] * 4);
    }
    cl_data_menu_quad_count = 0;
    for (corner = 0; corner < 4; corner++) {
        for (k = 0; k < 4; k++)
            quads[cl_data_menu_quad_count][k] = corner * 4 + k;
        cl_data_menu_quad_count++;
    }
    for (corner = 0; corner < 5; corner++) {
        memcpy(quads[cl_data_menu_quad_count], edge_quads[corner], sizeof edge_quads[0]);
        cl_data_menu_quad_count++;
    }
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[MENU_TEXTURE].gl_name);
    glBegin(GL_QUADS);
    if (lit == 0)
        alpha = (float)(alpha * 0.5);
    for (corner = 0; corner < cl_data_menu_quad_count; corner++) {
        for (k = 0; k < 4; k++) {
            int v = quads[corner][k];

            glColor4f(red, green, blue, (float)(alpha * cl_data_menu_alpha[v]));
            glTexCoord2fv(texcoord[v]);
            glVertex2fv(vertex[v]);
        }
    }
    glEnd();
}

/* Keep the start and the end of the selection inside the text. */
static void clamp_selection(void)
{
    if (cl_data_textinput_start < 0)
        cl_data_textinput_start = 0;
    if (strlen(cl_data_textinput_text) < (size_t)cl_data_textinput_start)
        cl_data_textinput_start = (int)strlen(cl_data_textinput_text);
    if (cl_data_textinput_end < 0)
        cl_data_textinput_end = 0;
    if (strlen(cl_data_textinput_text) < (size_t)cl_data_textinput_end)
        cl_data_textinput_end = (int)strlen(cl_data_textinput_text);
}

/* The item of the callback. A letter callback can change the index, so each access reads it
 * again. */
#define current_item (cl_data_menu_items[cl_data_menu_item_index])

/* Callback of a text field. On the first frame of the press, the field copies its value into
 * the text input. A click puts the cursor at the mouse, a key selects all text. While the field
 * stays pressed, it takes the mouse selection and the typed letters. Each frame writes the text
 * back to the value. When the press ends, the first 256 bytes of the text are cleared. */
void __cdecl menutextbox(void)
{
    cl_data_audio_input_blocked = 1;
    if (current_item.pressed_previous == 0) {
        if (current_item.field_type == 1)
            sprintf(cl_data_textinput_text, "%d", *current_item.value);
        else if (current_item.field_type == 2)
            sprintf(cl_data_textinput_text, "%u", *current_item.value);
        else if (current_item.field_type == 3)
            os_sprintf(cl_data_textinput_text, "%g", (double)*(float *)current_item.value);
        else
            strcpy(cl_data_textinput_text, (const char *)current_item.value);
        if (cl_data_input_mouse.left == 0 || cl_data_input_mouse_previous.left != 0) {
            cl_data_textinput_start = 0;
            cl_data_textinput_end = (int)strlen(cl_data_textinput_text);
        } else {
            cl_data_textinput_start = gettextinputposition();
            cl_data_textinput_end = cl_data_textinput_start;
        }
        clamp_selection();
    } else {
        int letter;

        if (cl_data_input_mouse.left != 0) {
            if (cl_data_input_mouse_previous.left == 0)
                cl_data_textinput_start = gettextinputposition();
            cl_data_textinput_end = gettextinputposition();
        }
        clamp_selection();
        /* When a letter ends the press, the loop stops. */
        for (letter = getinputletter(0); letter != -1; letter = current_item.pressed == 0 ? -1 : getinputletter(0))
            menu_textbox_putchar(letter);
    }
    if (current_item.field_type == 1)
        os_sscanf(cl_data_textinput_text, "%d", current_item.value);
    else if (current_item.field_type == 2)
        os_sscanf(cl_data_textinput_text, "%u", current_item.value);
    else if (current_item.field_type == 3)
        os_sscanf(cl_data_textinput_text, "%g", (float *)current_item.value);
    else
        strcpy((char *)current_item.value, cl_data_textinput_text);
    if (current_item.pressed == 0)
        memset(cl_data_textinput_text, 0, TEXT_KEEP);
}

/* Send the quad of one glyph: from x to right and from y to bottom. */
static void glyph_quad(const Font *font, int glyph, float x, double right, float y, double bottom)
{
    const float *uv = &font->coordinates[glyph * 8];
    float corner[2];

    corner[0] = x;
    corner[1] = y;
    glTexCoord2fv(uv);
    glVertex2fv(corner);
    corner[0] = (float)right;
    glTexCoord2fv(uv + 2);
    glVertex2fv(corner);
    corner[1] = (float)bottom;
    glTexCoord2fv(uv + 4);
    glVertex2fv(corner);
    corner[0] = x;
    glTexCoord2fv(uv + 6);
    glVertex2fv(corner);
}

/* Draw a text in a box: the text wraps into lines that fit the width, and the drawing stops at
 * the last line that fits the height. The flags are not used. */
void __cdecl drawtextbox(const char *text, float x, float y, float width, float height, float size,
                         uint32_t flags, float red, float green, float blue, float alpha)
{
    const Font *font = &cl_data_fonts[cl_data_fontnum];
    int i = 0, line = 0;
    float advance = 0.0f, line_y = 0.0f;

    (void)flags;
    formattextbox(text, width, height, size);
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[font->texture].gl_name);
    glBegin(GL_QUADS);
    glColor4f(red, green, blue, alpha);
    while (text[i] != '\0' && i < DRAW_LIMIT && (double)line_y + size <= height) {
        unsigned char c = (unsigned char)text[i];

        glyph_quad(font, c, (float)(x + advance), font->width[c] * size + ((double)x + advance),
                   (float)(y + line_y), (double)y + line_y + size);
        advance = (float)(font->width[c] * size + advance);
        if (i == cl_data_wrap_line_ends[line]) {
            advance = 0.0f;
            line_y = (float)(line_y + size);
            line++;
        }
        i++;
    }
    glEnd();
}

/* Draw the text of a text field with the cursor and the selection of the text input. A field
 * of kind 2 wraps its text in lines that fit the width, and stops at the last line that fits
 * the height. The cursor is the glyph '_' at the end of the selection. The selection is glyph 0
 * with half the alpha. */
void __cdecl drawmenutextfield(int index, const char *text, float x, float y, float width, float height,
                               float size, uint32_t flags, float red, float green, float blue, float alpha)
{
    const Font *font = &cl_data_fonts[cl_data_fontnum];
    int wrap = cl_data_menu_items[index].kind == KIND_TEXT_2;
    int i = 0, line = 0;
    float advance = 0.0f, line_y = 0.0f;

    (void)flags;
    if (wrap)
        formattextbox(text, width, height, size);
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[font->texture].gl_name);
    glBegin(GL_QUADS);
    while (text[i] != '\0' && i < DRAW_LIMIT && (double)line_y + size <= height) {
        unsigned char c = (unsigned char)text[i];
        float left = (float)(x + advance), top = (float)(y + line_y);
        double right = font->width[c] * size + ((double)x + advance);
        double bottom = (double)y + line_y + size;

        glColor4f(red, green, blue, alpha);
        glyph_quad(font, c, left, right, top, bottom);
        if (i == cl_data_textinput_end)
            glyph_quad(font, GLYPH_CURSOR, left, font->width[GLYPH_CURSOR] * size + ((double)x + advance), top,
                       bottom);
        if ((cl_data_textinput_start <= i && i < cl_data_textinput_end) ||
            (cl_data_textinput_end <= i && i < cl_data_textinput_start)) {
            glColor4f(red, green, blue, (float)(alpha * 0.5));
            glyph_quad(font, 0, left, right, top, bottom);
        }
        advance = (float)(font->width[c] * size + advance);
        if (wrap && i == cl_data_wrap_line_ends[line]) {
            advance = 0.0f;
            line_y = (float)(line_y + size);
            line++;
        }
        i++;
    }
    if (i == cl_data_textinput_end) {
        glColor4f(red, green, blue, alpha);
        glyph_quad(font, GLYPH_CURSOR, (float)(x + advance), font->width[GLYPH_CURSOR] * size + ((double)x + advance),
                   (float)(y + line_y), (double)y + line_y + size);
    }
    glEnd();
}

/* The text of a field value. A password shows a star for each character. */
static void field_text(const MenuItem *item, char *shown)
{
    size_t i;

    if (item->field_type == 1)
        sprintf(shown, "%d", *item->value);
    else if (item->field_type == 2)
        sprintf(shown, "%u", *item->value);
    else if (item->field_type == 3)
        os_sprintf(shown, "%g", (double)*(float *)item->value);
    else
        strcpy(shown, (const char *)item->value);
    if (item->password != 0)
        for (i = 0; i < strlen(shown); i++)
            shown[i] = '*';
}

/* Draw the label of a button or a toggle: grey that gets lighter as the item fades in, dark
 * grey on a disabled item. */
static void draw_label(const MenuItem *item)
{
    float shade = item->disabled == 0 ? (float)(0.875 - item->fade * 0.125) : 0.25f;

    drawtext(item->text, item->label_x, item->label_y, item->text_size, item->label_flags | TEXT_LABEL, shade,
             shade, shade, item->color[3]);
}

/* The brightness of a box: (lit + 1) / 16. */
static float box_light(const MenuItem *item)
{
    return (float)((float)(item->lit + 1) / 16.0);
}

/* A text field: a box, the value or the text that the player edits, and the label. */
static void draw_field(int index, const MenuItem *item)
{
    float light = box_light(item);

    drawbitmap(-1, item->x, item->y, item->width, item->height, item->text_flags, (float)(item->color[0] * light),
               (float)(item->color[1] * light), (float)(item->color[2] * light), item->color[3]);
    if (item->pressed == 0) {
        field_text(item, cl_data_textinput_shown);
        if (item->kind == KIND_TEXT)
            drawtext(cl_data_textinput_shown, item->x, item->y, item->text_size, item->text_flags | TEXT_SHADOW,
                     item->color[0], item->color[1], item->color[2], item->color[3]);
        if (item->kind == KIND_TEXT_2)
            drawtextbox(cl_data_textinput_shown, item->x, item->y, item->width, item->height, item->text_size,
                            item->text_flags, item->color[0], item->color[1], item->color[2], item->color[3]);
    } else {
        size_t i;

        strcpy(cl_data_textinput_shown, cl_data_textinput_text);
        if (item->password != 0)
            for (i = 0; i < strlen(cl_data_textinput_text); i++)
                cl_data_textinput_shown[i] = '*';
        drawmenutextfield(index, cl_data_textinput_shown, item->x, item->y, item->width, item->height,
                          item->text_size, item->text_flags, item->color[0], item->color[1], item->color[2],
                          item->color[3]);
    }
    drawtext(item->text, item->label_x, item->label_y, item->label_size, TEXT_SHADOW, item->color[0],
             item->color[1], item->color[2], item->color[3]);
}

/* A slider: the bar, the knob at the value, the label and the value at the right end. */
static void draw_slider(const MenuItem *item)
{
    float light = box_light(item);
    double place;

    drawbitmap(-1, item->x, item->y, item->width, item->height, 0, (float)(item->color[0] * light),
               (float)(item->color[1] * light), (float)(item->color[2] * light), item->color[3]);
    place = ((double)item->width - SLIDER_KNOB) * (double)(*item->value - item->slider_min) /
            (double)(item->slider_max - item->slider_min);
    drawbitmap(-1, (float)((double)cl_ftoi(place) + item->x), item->y, (float)SLIDER_KNOB, item->height, 0, 1.0f,
               1.0f, 1.0f, 1.0f);
    drawtext(item->text, item->label_x, item->label_y, item->label_size, TEXT_SHADOW, item->color[0],
             item->color[1], item->color[2], item->color[3]);
    drawtext("/i", (float)(item->x + item->width), item->label_y, item->label_size, 2, item->color[0],
             item->color[1], item->color[2], item->color[3], *item->value);
}

/* A toggle: the label, the button, a small box at the right end and an X in it when set. */
static void draw_toggle(const MenuItem *item)
{
    float box_x = (float)((double)item->x + item->width - TOGGLE_BOX);

    draw_label(item);
    drawmenubutton2(item->x, item->y, item->width, item->height, item->lit, 0, item->color[0], item->color[1],
                    item->color[2], item->color[3]);
    drawlinebox(box_x, item->label_y, item->text_size, item->text_size, 5, (float)(item->color[0] * 0.5),
                (float)(item->color[1] * 0.5), (float)(item->color[2] * 0.5), item->color[3]);
    if (*item->value != 0)
        drawtext("X", box_x, item->label_y, item->text_size, 0x65, item->color[0], item->color[1], item->color[2],
                 item->color[3]);
}

/* Draw all items that are not hidden. An item with a bitmap shows only the bitmap, at half
 * brightness when the mouse is not over it. Then each plain item under the mouse gets a second
 * lit button at half alpha. */
void __cdecl drawmenuitems(void)
{
    int i, k;

    for (i = 0; i < cl_data_menu_item_count; i++) {
        const MenuItem *item = &cl_data_menu_items[i];

        if (item->hidden != 0)
            continue;
        if (item->links[0] != -1) {
            if (item->hover == 0)
                drawbitmap(item->links[0], item->x, item->y, item->width, item->height, 0,
                           (float)(item->color[0] * 0.5), (float)(item->color[1] * 0.5),
                           (float)(item->color[2] * 0.5), item->color[3]);
            else
                drawbitmap(item->links[0], item->x, item->y, item->width, item->height, 0, item->color[0],
                           item->color[1], item->color[2], item->color[3]);
        } else if (item->kind == KIND_TEXT || item->kind == KIND_TEXT_2) {
            draw_field(i, item);
        } else if (item->kind == KIND_SLIDER) {
            draw_slider(item);
        } else if (item->kind == KIND_TOGGLE) {
            draw_toggle(item);
        } else {
            draw_label(item);
            for (k = 0; k < item->num_info; k++) {
                const MenuInfo *info = &item->info[k];

                drawtext(info->text, info->x, info->y, info->size, info->flags | TEXT_LABEL, info->color[0],
                         info->color[1], info->color[2], info->color[3]);
            }
            drawmenubutton2(item->x, item->y, item->width, item->height, item->lit, 0, item->color[0],
                            item->color[1], item->color[2], item->color[3]);
        }
    }
    for (i = 0; i < cl_data_menu_item_count; i++) {
        const MenuItem *item = &cl_data_menu_items[i];

        if (item->hover != 0 && item->links[0] == -1)
            drawmenubutton2(item->x, item->y, item->width, item->height, 1, 0, item->color[0], item->color[1],
                            item->color[2], (float)(item->color[3] * 0.5));
    }
}
