/* The system events of a frame and the letters of the text input.
 *
 * checksystemmessages reads the SDL events: the wheel, the key presses (up to 1024 for each
 * frame) and the request to quit. getinputletter turns the key presses into letters and the
 * keys that edit the text into codes.
 */
#include "input/key_events.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "system_libs.h"
#include "client.h"
#include "setup/game.h"
#include "sr_runtime.h"

#define EVENT_QUIT 0x100
#define EVENT_KEY_DOWN 0x300
#define EVENT_WHEEL 0x403
#define MAX_KEY_EVENTS 0x400
#define SHIFT_MODS 3       /* the left and the right shift */
#define MESSAGE_DELAY_MS 10
#define FIRST_LETTER 0x20  /* the space */
#define LAST_LETTER 0x7a   /* z */
#define LETTER_DELETE 10
#define LETTER_DONE 11
#define LETTER_NEXT 12
#define LETTER_PREVIOUS 13
#define NO_LETTER (-1)

/* The part of an SDL event that the client reads. A key event has the scancode, the key code
 * and the modifiers. A wheel event has its y steps at the place of the key code. */
typedef struct SdlEvent {
    uint32_t type;
    uint8_t pad_04[12];
    int scancode;
    int key;
    uint16_t mod;
    uint8_t pad_1a[30];
} SdlEvent;

enum {
    SCAN_RETURN = 0x28, SCAN_ESCAPE = 0x29, SCAN_BACKSPACE = 0x2a, SCAN_TAB = 0x2b,
    SCAN_INSERT = 0x49, SCAN_HOME = 0x4a, SCAN_DELETE = 0x4c, SCAN_END = 0x4d,
    SCAN_RIGHT = 0x4f, SCAN_LEFT = 0x50, SCAN_LEFT_SHIFT = 0xe1,
};

/* Read the events of this frame. The wheel steps add to the mouse, the key presses go into
 * the key list and a quit request sets the back flag of the menus. */
void __cdecl checksystemmessages(void)
{
    SdlEvent event;

    if (cl_data_message_delay != 0)
        SDL_Delay(MESSAGE_DELAY_MS);
    cl_data_key_event_count = 0;
    cl_data_key_event_next = 0;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == EVENT_WHEEL) {
            if (event.key < 0)
                cl_data_input_mouse.wheel_down -= event.key;
            if (event.key > 0)
                cl_data_input_mouse.wheel_up += event.key;
        }
        if (event.type == EVENT_KEY_DOWN && cl_data_key_event_count < MAX_KEY_EVENTS) {
            KeyEvent *key = &cl_data_key_events[cl_data_key_event_count++];

            key->scancode = event.scancode;
            key->key = event.key;
            key->mod = event.mod;
        }
        if (event.type == EVENT_QUIT)
            cl_data_audio_menu_back = 1;
    }
    updateplayticket();
}

/* The letter of a key with shift on a US keyboard. */
static int shifted(int letter)
{
    static const char from[] = "0123456789-=/,.;'";
    static const char to[] = ")!@#$%^&*(_+?<>:\"";
    const char *at;

    if (letter >= 'a' && letter <= 'z')
        return toupper(letter);
    at = strchr(from, letter);
    return at != NULL && *at != '\0' ? to[at - from] : letter;
}

/* Keep the start and the end of the selection in the text. */
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

/* The next letter of the key presses of this frame, or NO_LETTER when there is none. Backspace
 * and Delete give LETTER_DELETE, Return and Escape give LETTER_DONE, Tab gives LETTER_NEXT
 * (LETTER_PREVIOUS with shift). Insert switches the overwrite mode. Home, End and the arrows
 * move the cursor: without shift the selection ends at the cursor. Keys with a code out of
 * FIRST_LETTER to LAST_LETTER give nothing. The argument is not used. */
int __cdecl getinputletter(int wait)
{
    (void)wait;
    for (;;) {
        const KeyEvent *key;
        int shift;

        clamp_selection();
        if (cl_data_key_event_next >= cl_data_key_event_count)
            return NO_LETTER;
        key = &cl_data_key_events[cl_data_key_event_next++];
        shift = (key->mod & SHIFT_MODS) != 0;
        switch (key->scancode) {
        case SCAN_DELETE:
        case SCAN_BACKSPACE:
            return LETTER_DELETE;
        case SCAN_LEFT_SHIFT:
            continue;
        case SCAN_RETURN:
        case SCAN_ESCAPE:
            return LETTER_DONE;
        case SCAN_TAB:
            return shift ? LETTER_PREVIOUS : LETTER_NEXT;
        case SCAN_INSERT:
            cl_data_textinput_overwrite ^= 1;
            continue;
        case SCAN_HOME:
            cl_data_textinput_end = 0;
            break;
        case SCAN_END:
            cl_data_textinput_end = (int)strlen(cl_data_textinput_text);
            break;
        case SCAN_LEFT:
            if (cl_data_textinput_end > 0)
                cl_data_textinput_end--;
            break;
        case SCAN_RIGHT:
            if ((size_t)cl_data_textinput_end < strlen(cl_data_textinput_text))
                cl_data_textinput_end++;
            break;
        default:
            if (key->key >= FIRST_LETTER && key->key <= LAST_LETTER)
                return shift ? shifted(key->key) : key->key;
            continue;
        }
        if (!shift)
            cl_data_textinput_start = cl_data_textinput_end;
    }
}
