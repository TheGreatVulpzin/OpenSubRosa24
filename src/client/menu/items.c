/* The menu items. Each menu frame fills the list again: createmenuitem, createmenubutton and
 * createmenuitemempty add an item at the end, and setmenuitem and setmenuiteminfo change the
 * last item. checkmenuitems then finds the item under the mouse and the items of the keys that
 * went down, and calls their callbacks. Item 0 is the back button of each menu: the menu loops
 * end when it is pressed. */
#include "menu/items.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define ITEMS 256
#define TEXT_LIMIT 0x3f
#define LENGTH_LIMIT 1000

enum { KIND_BUTTON = 0, KIND_TOGGLE = 3, KIND_CHOICE = 4 };

typedef void(__cdecl *MenuCallback)(void);

/* Copy the bits of a float. A float load can change the bits of a signaling NaN. */
static void copy_bits(float *to, const float *from)
{
    memcpy(to, from, sizeof *to);
}

/* The width of the text in the current font: the sum of the character widths times the scale.
 * Each step of the sum rounds to a float. */
static float text_width(const char *text, float scale, int limit)
{
    const Font *font = &cl_data_fonts[cl_data_fontnum];
    float width = 0.0f;
    int i;

    for (i = 0; text[i] != '\0' && i < limit; i++)
        width = (float)(font->width[(unsigned char)text[i]] * scale + width);
    return width;
}

/* The fields that createmenuitem and createmenubutton set in the same way. */
static void item_defaults(MenuItem *item, const char *text)
{
    static const float spare[8] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    int i;

    item->num_info = 0;
    item->kind = KIND_BUTTON;
    item->key = 0;
    item->key2 = 0;
    item->repeat = 0;
    item->disabled = 0;
    item->hover = 0;
    item->hidden = 0;
    item->lit = 0;
    item->mask = 0;
    item->field_type = 0;
    item->length = (int)strlen(text);
    item->value = 0;
    item->callback = 0;
    for (i = 0; i < 4; i++)
        item->links[i] = -1;
    memcpy(item->spare, spare, sizeof spare);
    item->cursor = 0;
    item->spare_278 = 0;
    item->password = 0;
}

/* Clear the press of all 256 items. */
void __cdecl resetmenuitems(void)
{
    int i;

    for (i = 0; i < ITEMS; i++) {
        cl_data_menu_items[i].pressed = 0;
        cl_data_menu_items[i].pressed_previous = 0;
    }
}

/* Add an item with no text and no size. The list has no size limit: item 256 is past the
 * array. */
void __cdecl createmenuitemempty(void)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_count];
    int i;

    memset(item->text, 0, sizeof item->text);
    item->x = 0.0f;
    item->y = 0.0f;
    item->text_size = 0.0f;
    item->text_flags = 0;
    for (i = 0; i < 4; i++)
        item->color[i] = 0.0f;
    item->kind = KIND_BUTTON;
    item->width = 0.0f;
    item->height = 0.0f;
    item->key = 0;
    item->key2 = 0;
    item->repeat = 0;
    item->disabled = 0;
    item->hover = 0;
    item->lit = 0;
    item->mask = 0;
    item->field_type = 0;
    item->length = 0;
    item->value = 0;
    item->callback = 0;
    for (i = 0; i < 4; i++)
        item->links[i] = -1;
    item->cursor = 0;
    item->spare_278 = 0;
    item->password = 0;
    cl_data_menu_item_count++;
}

/* Add an item with a text. The hit box is as wide as the first 63 characters of the text and
 * as high as the text size. Flags: 1 centers the text on x, 2 puts its end at x, 4 centers it
 * on y, 8 puts its bottom at y. */
void __cdecl createmenuitem(const char *text, float x, float y, float size, uint32_t flags,
                            float red, float green, float blue, float alpha)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_count];
    float width = text_width(text, size, TEXT_LIMIT);

    if (flags & 1)
        x = (float)(x - width * 0.5);
    else if (flags & 2)
        x = (float)(x - width);
    if (flags & 4)
        y = (float)(y - size * 0.5);
    else if (flags & 8)
        y = (float)(y - size);
    strcpy(item->text, text);
    item->label_x = x;
    item->label_y = y;
    copy_bits(&item->label_size, &size);
    item->x = x;
    item->y = y;
    copy_bits(&item->text_size, &size);
    item->text_flags = 0;
    copy_bits(&item->color[0], &red);
    copy_bits(&item->color[1], &green);
    copy_bits(&item->color[2], &blue);
    copy_bits(&item->color[3], &alpha);
    item->width = width;
    copy_bits(&item->height, &size);
    item_defaults(item, text);
    cl_data_menu_item_count++;
}

/* Add an item with a given hit box and the text in its center. The flags place the box on x
 * and y as in createmenuitem. */
void __cdecl createmenubutton(const char *text, float x, float y, float width, float height, float size,
                              uint32_t flags, float red, float green, float blue, float alpha)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_count];

    if (flags & 1)
        x = (float)(x - width * 0.5);
    else if (flags & 2)
        x = (float)(x - width);
    if (flags & 4)
        y = (float)(y - height * 0.5);
    else if (flags & 8)
        y = (float)(y - height);
    strcpy(item->text, text);
    item->label_x = (float)(width * 0.5 + x);
    item->label_y = (float)(height * 0.5 + y);
    copy_bits(&item->label_size, &size);
    item->label_flags = 5;
    item->x = x;
    item->y = y;
    copy_bits(&item->text_size, &size);
    item->text_flags = flags;
    copy_bits(&item->color[0], &red);
    copy_bits(&item->color[1], &green);
    copy_bits(&item->color[2], &blue);
    copy_bits(&item->color[3], &alpha);
    copy_bits(&item->width, &width);
    copy_bits(&item->height, &height);
    item_defaults(item, text);
    cl_data_menu_item_count++;
}

/* Add an info line to the last item. An item holds four lines: more lines are lost. With no
 * items, the line goes to the record before the list. */
void __cdecl setmenuiteminfo(const char *text, float x, float y, float size, uint32_t flags,
                             float red, float green, float blue, float alpha)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_count - 1];
    MenuInfo *info;

    if (item->num_info >= 4)
        return;
    info = &item->info[item->num_info];
    strcpy(info->text, text);
    copy_bits(&info->x, &x);
    copy_bits(&info->y, &y);
    copy_bits(&info->size, &size);
    info->flags = flags;
    copy_bits(&info->color[0], &red);
    copy_bits(&info->color[1], &green);
    copy_bits(&info->color[2], &blue);
    copy_bits(&info->color[3], &alpha);
    item->num_info++;
}

/* Find the item under the mouse and the items of the keys that went down, then call the
 * callback of each pressed item. A button is pressed for one frame only. A click clears the
 * presses of the frame before. The loop index stays in menu_item_index, so a callback knows
 * its item. */
void __cdecl checkmenuitems(void)
{
    const InputMouse *mouse = &cl_data_input_mouse;
    int click = mouse->left != 0 && cl_data_input_mouse_previous.left == 0;
    int count = cl_data_menu_item_count;
    int i;

    for (i = 0; i < count; i++)
        if (click || cl_data_menu_items[i].repeat != 0)
            cl_data_menu_items[i].pressed = 0;
    cl_data_fonts[cl_data_fontnum].cursor = 0;
    for (i = 0; i < count; i++) {
        MenuItem *item = &cl_data_menu_items[i];
        float mx = (float)mouse->x, my = (float)mouse->y;

        item->hover = 0;
        item->lit = 0;
        /* x <= mx: a NaN gives false. */
        if (item->x <= mx && (double)mx < (double)item->x + item->width &&
            item->y <= my && (double)my < (double)item->y + item->height) {
            if (item->disabled == 0) {
                item->hover = 1;
                cl_data_fonts[cl_data_fontnum].cursor = item->cursor;
                if (mouse->left != 0 && cl_data_input_mouse_previous.left == 0)
                    item->pressed = 1;
            }
        }
        if (item->disabled == 0) {
            if (cl_data_audio_input_blocked == 0 && cl_data_audio_keys[item->key] != 0 &&
                (item->key2 == 0 || cl_data_audio_keys[item->key2] != 0) &&
                (cl_data_audio_keys_previous[item->key] == 0 || item->repeat != 0))
                item->pressed = 1;
            if (item->pressed != 0)
                item->lit = 1;
        }
        if (item->kind == KIND_TOGGLE && (*(uint32_t *)item->value & item->mask) == item->mask)
            item->lit = 1;
        if (item->kind == KIND_CHOICE && *item->value == (int)item->mask)
            item->lit = 1;
        if (item->lit == 1)
            item->fade = (float)((-1.0 - item->fade) * 0.125 + item->fade);
        else
            item->fade = (float)((1.0 - item->fade) * 0.125 + item->fade);
    }
    cl_data_audio_input_blocked = 0;
    /* A callback can change the index, so each access reads it again. */
    for (cl_data_menu_item_index = 0; cl_data_menu_item_index < cl_data_menu_item_count;
         cl_data_menu_item_index++) {
        if (cl_data_menu_items[cl_data_menu_item_index].pressed != 0 &&
            cl_data_menu_items[cl_data_menu_item_index].callback != 0) {
            ((MenuCallback)cl_data_menu_items[cl_data_menu_item_index].callback)();
            if (cl_data_menu_items[cl_data_menu_item_index].kind == KIND_BUTTON)
                cl_data_menu_items[cl_data_menu_item_index].pressed = 0;
        }
        cl_data_menu_items[cl_data_menu_item_index].pressed_previous =
            cl_data_menu_items[cl_data_menu_item_index].pressed;
    }
}

/* Callback of a bit toggle: flip the bits of the mask in the value. */
void __cdecl menutoggle(void)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_index];

    *(uint32_t *)item->value ^= item->mask;
    item->pressed = 0;
}

/* Callback of a choice: write the mask to the value. */
void __cdecl menuset(void)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_index];

    *item->value = (int)item->mask;
    item->pressed = 0;
}

/* The width of the text in the current font, for at most 1000 characters. */
float __cdecl text_getlength(const char *text, float scale)
{
    return text_width(text, scale, LENGTH_LIMIT);
}
