/* The text input of the menus: the letters of a text field, the selection and the line wrap.
 * The text input holds one text that the player edits, the start and the end of the selection
 * (the end is the cursor) and the overwrite mode. */
#include "menu/textinput.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define TEXT_LIMIT 0x2000

/* The width of a letter in the current font. */
static float glyph_width(unsigned char c)
{
    return cl_data_fonts[cl_data_fontnum].width[c];
}

/* Add the index of the last letter of a line to the line ends. */
static void add_line_end(int index)
{
    cl_data_wrap_line_ends[cl_data_wrap_line_count] = index;
    cl_data_wrap_line_count++;
}

/* Put one letter at the cursor. In insert mode the rest of the text moves one place on. The
 * cursor moves after the letter, and the selection ends there. A cursor at 0x2000 or more takes
 * nothing. */
void __cdecl addmenuinputchar(char letter)
{
    int i;

    if (cl_data_textinput_end >= TEXT_LIMIT)
        return;
    if (cl_data_textinput_overwrite == 0)
        for (i = (int)strlen(cl_data_textinput_text) + 1; i > cl_data_textinput_end; i--)
            cl_data_textinput_text[i] = cl_data_textinput_text[i - 1];
    cl_data_textinput_text[cl_data_textinput_end] = letter;
    cl_data_textinput_start = cl_data_textinput_end + 1;
    cl_data_textinput_end = cl_data_textinput_start;
}

/* Delete the selected letters. The cursor goes to the start of the selection, at most to the
 * end of the text. Return 1 when the selection held a letter. */
int __cdecl deleteselectedtext(void)
{
    int kept = 0, deleted = 0;
    unsigned cursor = (unsigned)cl_data_textinput_end, i;

    for (i = 0; i < strlen(cl_data_textinput_text); i++) {
        int selected = ((int)i >= cl_data_textinput_start && (int)i < cl_data_textinput_end) ||
                       ((int)i >= cl_data_textinput_end && (int)i < cl_data_textinput_start);

        if (!selected) {
            cl_data_textinput_shown[kept++] = cl_data_textinput_text[i];
        } else {
            if ((int)i < (int)cursor)
                cursor = i;
            deleted = 1;
        }
    }
    cl_data_textinput_shown[kept] = '\0';
    strcpy(cl_data_textinput_text, cl_data_textinput_shown);
    cl_data_textinput_end = (int)cursor;
    if (strlen(cl_data_textinput_text) < (unsigned)cl_data_textinput_end)
        cl_data_textinput_end = (int)strlen(cl_data_textinput_text);
    cl_data_textinput_start = cl_data_textinput_end;
    return deleted;
}

/* Wrap a text into lines that fit a width, in the current font at a size. Each line end is the
 * index of the last letter of the line. A word that does not fit goes to the next line, and a
 * word longer than a line breaks before the letter that does not fit. A newline ends a line.
 * The height is not used. A width below the width of one letter never ends: the wrap adds the
 * same line end again and again. */
void __cdecl formattextbox(const char *text, float width, float height, float size)
{
    float line = 0.0f, word;
    int end = 0, i = 0;

    (void)height;
    cl_data_wrap_line_count = 0;
    while (text[end] != '\0') {
        for (word = 0.0f; text[i] != '\0' && text[i] != '\n' && text[i] != ' ' &&
                          ((double)line + word) + glyph_width((unsigned char)text[i]) * size < width;) {
            unsigned char c = (unsigned char)text[i];

            i++;
            word = (float)(glyph_width(c) * size + word);
        }
        if (text[i] == '\0') {
            end = i;
        } else if (text[i] == '\n') {
            add_line_end(i);
            line = 0.0f;
            i++;
            end = i;
        } else if (text[i] == ' ') {
            word = (float)(glyph_width(' ') * size + word);
            line = (float)(line + word);
            end = i;
            i++;
        } else if (line <= 0.0) {
            add_line_end(i - 1);
            line = 0.0f;
            end = i;
        } else {
            add_line_end(end);
            line = 0.0f;
            i = end + 1;
            end = i;
        }
    }
    add_line_end(end);
}

/* The index of the letter under the mouse in the pressed text field. A field of kind 2 wraps
 * its text first. The search goes letter by letter: it stops at the first letter whose right
 * edge is past the mouse on a line below the mouse. In a wrapped field, the end of a line stops
 * it too when the mouse is right of that line. It also stops at the end of the text and at the
 * length of the field. */
int __cdecl gettextinputposition(void)
{
    const MenuItem *item = &cl_data_menu_items[cl_data_menu_item_index];
    float mx = (float)((double)cl_data_input_mouse.x - item->x);
    float my = (float)((double)cl_data_input_mouse.y - item->y);
    float right = 0.0f, bottom;
    int i = 0, line = 0;

    if (item->kind == 2)
        formattextbox(cl_data_textinput_text, item->width, item->height, item->text_size);
    bottom = item->text_size;
    for (;;) {
        if (cl_data_textinput_text[i] == '\0')
            return i;
        if (i >= item->length)
            break;
        right = (float)(glyph_width((unsigned char)cl_data_textinput_text[i]) * item->text_size + right);
        if (my < bottom && mx < right)
            return i;
        if (item->kind == 2 && i == cl_data_wrap_line_ends[line]) {
            if (my < bottom && right <= mx)
                return i;
            right = 0.0f;
            bottom = (float)(bottom + item->text_size);
            line++;
        }
        i++;
    }
    return i;
}

/* The letter codes of the text input. */
enum {
    LETTER_DELETE = 9, LETTER_BACKSPACE = 10, LETTER_ENTER = 11, LETTER_NEXT = 12, LETTER_PREVIOUS = 13,
    LETTER_FIRST = 0x20
};

/* Step the index one item on or back. The forward step wraps at the end of the list, the back
 * step at the start. */
static int step_item(int i, int step)
{
    i += step;
    if (step > 0 && i >= cl_data_menu_item_count)
        i = 0;
    if (step < 0 && i < 0)
        i = cl_data_menu_item_count - 1;
    return i;
}

/* Move the press from the pressed item to the next text field of kind 1, forward or back. The
 * search stops at the item it started from. */
static void move_to_field(int step)
{
    int current = cl_data_menu_item_index, i;

    for (i = step_item(current, step); i != current; i = step_item(i, step)) {
        if (cl_data_menu_items[i].kind == 1) {
            cl_data_menu_items[current].pressed = 0;
            cl_data_menu_items[i].pressed = 1;
            return;
        }
    }
}

/* Apply one letter code to the text of the pressed text field. A letter replaces the selection
 * and goes in at the cursor while the text is shorter than the field length. Delete and
 * backspace remove the selection, or one letter after or before the cursor when nothing is
 * selected. Enter ends the press. Next and previous move the press to another text field. */
void __cdecl menu_textbox_putchar(int letter)
{
    int i;

    if (letter >= LETTER_FIRST) {
        deleteselectedtext();
        if (strlen(cl_data_textinput_text) < (unsigned)cl_data_menu_items[cl_data_menu_item_index].length)
            addmenuinputchar((char)letter);
    }
    if (letter == LETTER_DELETE && deleteselectedtext() == 0)
        for (i = cl_data_textinput_end; cl_data_textinput_text[i] != '\0'; i++)
            cl_data_textinput_text[i] = cl_data_textinput_text[i + 1];
    if (letter == LETTER_BACKSPACE && deleteselectedtext() == 0 && cl_data_textinput_end > 0) {
        for (i = cl_data_textinput_end - 1; cl_data_textinput_text[i] != '\0'; i++)
            cl_data_textinput_text[i] = cl_data_textinput_text[i + 1];
        cl_data_textinput_start = cl_data_textinput_end - 1;
        cl_data_textinput_end = cl_data_textinput_start;
    }
    if (letter == LETTER_NEXT)
        move_to_field(1);
    if (letter == LETTER_PREVIOUS)
        move_to_field(-1);
    if (letter == LETTER_ENTER)
        cl_data_menu_items[cl_data_menu_item_index].pressed = 0;
}
