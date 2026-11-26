/* The menu items. Each menu frame fills the list again: createmenuitem, createmenubutton and
 * createmenuitemempty add an item at the end, and setmenuitem and setmenuiteminfo change the
 * last item. checkmenuitems then finds the item under the mouse and the items of the keys that
 * went down, and calls their callbacks. Item 0 is the back button of each menu: the menu loops
 * end when it is pressed. */
#include "menu/items.h"

#include <stdarg.h>
#include <string.h>

#include "client.h"
#include "sr_runtime.h"
#include "draw.h"

#define ITEMS 256
#define TEXT_LIMIT 0x3f
#define LENGTH_LIMIT 1000
#define KEY_LEFT_CTRL 0xe0
#define KEY_NAME_BYTES 16

enum { KIND_BUTTON = 0, KIND_TEXT = 1, KIND_TEXT_2 = 2, KIND_TOGGLE = 3, KIND_CHOICE = 4, KIND_SLIDER = 5 };

/* The options of setmenuitem. */
enum {
    OPTION_KEY = 1, OPTION_KEY2 = 2, OPTION_LIT = 3, OPTION_TEXT = 4, OPTION_TEXT_2 = 5,
    OPTION_INT = 6, OPTION_INT_2 = 7, OPTION_FLOAT = 8, OPTION_CALLBACK = 9, OPTION_TOGGLE = 10,
    OPTION_CHOICE = 11, OPTION_REPEAT = 12, OPTION_LINK = 13, OPTION_CURSOR = 15, OPTION_SPARE = 16,
    OPTION_PASSWORD = 17, OPTION_SPARE_COPY = 18, OPTION_SLIDER = 19, OPTION_DISABLED = 20,
    OPTION_LABEL = 21, OPTION_HIDDEN = 23
};

typedef void(__cdecl *MenuCallback)(void);

/* The callbacks that setmenuitem stores. The item holds the image address of each function. */
#define menutextbox_address ((void *)menutextbox)
#define menutoggle_address ((void *)menutoggle)
#define menuset_address ((void *)menuset)
#define menuslider_address ((void *)menuslider)

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

/* Fill the names of the keys that the menus show. The key indexes are SDL scancodes. Letters
 * and digits get one character. The names of the other keys do not change. */
void __cdecl setupmenuitems(void)
{
    static const struct {
        int key;
        const char *name;
    } names[] = {
        {80, "Left"}, {79, "Right"}, {82, "Up"}, {81, "Down"}, {44, "Space"}, {40, "Enter"},
        {93, "Center"}, {224, "Ctrl"}, {226, "Alt"}, {225, "Shift"}, {54, "Comma"}, {55, "Period"},
        {95, "Home"}, {97, "PgUp"}, {89, "End"}, {91, "PgDn"}, {87, "Plus"}, {86, "Minus"},
        {98, "Ins"}, {99, "Del"}, {0, "None"},
    };
    char *table = cl_data_audio_key_names;
    int i;

    for (i = 0; i < 26; i++) {
        table[(4 + i) * KEY_NAME_BYTES] = (char)('A' + i);
        table[(4 + i) * KEY_NAME_BYTES + 1] = '\0';
    }
    /* Keys 30 to 38 are 1 to 9, and key 39 is 0. */
    for (i = 0; i < 10; i++) {
        table[(30 + i) * KEY_NAME_BYTES] = (char)('0' + (i + 1) % 10);
        table[(30 + i) * KEY_NAME_BYTES + 1] = '\0';
    }
    for (i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        strcpy(table + names[i].key * KEY_NAME_BYTES, names[i].name);
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

/* Make the item a text field with its value buffer and size. The label goes above the field. */
static void text_field(MenuItem *item, int kind, int field_type, va_list *args)
{
    item->kind = kind;
    item->field_type = field_type;
    item->value = va_arg(*args, int *);
    item->length = va_arg(*args, int);
    item->callback = menutextbox_address;
    copy_bits(&item->label_x, &item->x);
    item->label_y = (float)((double)item->y - item->text_size);
}

/* Set one option of the last item. The arguments after the option depend on it. Options 14
 * and 22 do nothing. With no items, nothing changes. */
void __cdecl setmenuitem(int option, ...)
{
    MenuItem *item;
    va_list args;

    if (cl_data_menu_item_count == 0)
        return;
    item = &cl_data_menu_items[cl_data_menu_item_count - 1];
    va_start(args, option);
    switch (option) {
    case OPTION_KEY: item->key = va_arg(args, int); break;
    case OPTION_KEY2: item->key2 = va_arg(args, int); break;
    case OPTION_LIT: item->lit = va_arg(args, int); break;
    case OPTION_TEXT: text_field(item, KIND_TEXT, 0, &args); break;
    case OPTION_TEXT_2: text_field(item, KIND_TEXT_2, 0, &args); break;
    case OPTION_INT: text_field(item, KIND_TEXT, 1, &args); break;
    case OPTION_INT_2: text_field(item, KIND_TEXT, 2, &args); break;
    case OPTION_FLOAT: text_field(item, KIND_TEXT, 3, &args); break;
    case OPTION_CALLBACK: item->callback = va_arg(args, void *); break;
    case OPTION_TOGGLE:
        item->kind = KIND_TOGGLE;
        item->value = va_arg(args, int *);
        item->mask = va_arg(args, uint32_t);
        item->callback = menutoggle_address;
        item->label_flags = 4;
        item->label_x = (float)(item->x + 8.0);
        break;
    case OPTION_CHOICE:
        item->kind = KIND_CHOICE;
        item->value = va_arg(args, int *);
        item->mask = va_arg(args, uint32_t);
        item->callback = menuset_address;
        break;
    case OPTION_REPEAT: item->repeat = 1; break;
    case OPTION_LINK: {
        int index = va_arg(args, int);

        /* The index is not checked: an index outside 0 to 3 writes outside links. */
        item->links[index] = va_arg(args, int);
        if (index == 0)
            item->links[1] = item->links[2] = item->links[3] = item->links[0];
        break;
    }
    case OPTION_CURSOR: item->cursor = va_arg(args, int); break;
    case OPTION_SPARE: item->spare_278 = va_arg(args, int); break;
    case OPTION_PASSWORD: item->password = 1; break;
    case OPTION_SPARE_COPY: memcpy(item->spare, va_arg(args, const void *), sizeof item->spare); break;
    case OPTION_SLIDER:
        item->kind = KIND_SLIDER;
        item->field_type = 1;
        item->value = va_arg(args, int *);
        item->slider_min = va_arg(args, int);
        item->slider_max = va_arg(args, int);
        item->slider_step = va_arg(args, int);
        item->callback = menuslider_address;
        copy_bits(&item->label_x, &item->x);
        item->label_y = (float)((double)item->y - item->text_size);
        break;
    case OPTION_DISABLED:
        item->disabled = 1;
        item->color[0] = (float)(item->color[0] * 0.5);
        item->color[1] = (float)(item->color[1] * 0.5);
        item->color[2] = (float)(item->color[2] * 0.5);
        break;
    case OPTION_LABEL:
        item->label_x = (float)va_arg(args, double);
        item->label_y = (float)va_arg(args, double);
        item->label_size = (float)va_arg(args, double);
        item->label_flags = 0;
        break;
    case OPTION_HIDDEN: item->hidden = 1; break;
    }
    va_end(args);
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
            if (cl_data_audio_input_blocked == 0 && cl_data_keyboard[item->key] != 0 &&
                (item->key2 == 0 || cl_data_keyboard[item->key2] != 0) &&
                (cl_data_prevkeyboard[item->key] == 0 || item->repeat != 0))
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

/* Callback of a slider: set the value from the mouse position in the bar. The bar starts 4
 * units after x and is 8 units shorter than the item. With left Ctrl held, the value goes to
 * the nearest step. The item stays pressed while the left button is down. */
void __cdecl menuslider(void)
{
    MenuItem *item = &cl_data_menu_items[cl_data_menu_item_index];
    float t = (float)(((double)cl_data_input_mouse.x - ((double)item->x + 4.0)) / ((double)item->width - 8.0));

    /* A NaN position stays NaN. */
    if (t < 0.0)
        t = 0.0f;
    if (t > 1.0)
        t = 1.0f;
    *item->value = cl_ftoi((double)(item->slider_max - item->slider_min) * t + (double)item->slider_min);
    if (cl_data_keyboard[KEY_LEFT_CTRL] != 0 && item->slider_step > 0 && *item->value < item->slider_max) {
        *item->value = (item->slider_step >> 1) + *item->value;
        *item->value /= item->slider_step;
        *item->value *= item->slider_step;
    }
    if (*item->value > item->slider_max)
        *item->value = item->slider_max;
    item->hover = 1;
    if (cl_data_input_mouse.left == 0)
        item->pressed = 0;
}

/* The width of the text in the current font, for at most 1000 characters. */
float __cdecl text_getlength(const char *text, float scale)
{
    return text_width(text, scale, LENGTH_LIMIT);
}
