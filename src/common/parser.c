/* The text parser of the game.
 *
 * loadtextfile reads a file into a global buffer. The other functions move one cursor
 * (Parser.pos) over the bytes: a key search puts the cursor after the key, and the readers
 * take a number or a line from the cursor.
 */
#include "parser.h"

#include <string.h>

#include "sr_os.h"
#include "sr_runtime.h"

#define g_parser (*SR_AT(Parser, SR_RVA_PARSER))

/* Reserves one byte for the final 0. A file that exceeds the buffer gives no text.
 * When the file does not open, the buffer keeps its old bytes. */
void loadtextfile(const char *path)
{
    Parser *p = &g_parser;
    int c;

    p->size = 0;
    p->file = os_fopen(path, "rb");
    if (p->file) {
        for (c = os_fgetc(p->file); c != -1 && p->size < sizeof p->text - 1;
             c = os_fgetc(p->file))
            p->text[p->size++] = (char)c;
        if (c != -1)
            p->size = 0;
        p->text[p->size] = 0;
        os_fclose(p->file);
    }
    p->pos = 0;
}

/* Returns 1 when the text at the cursor starts with the key. The cursor does not move. */
int checkstring(const char *key)
{
    Parser *p = &g_parser;
    int i = 0;

    if (p->pos < p->size) {
        while (key[i] != 0 && key[i] == p->text[p->pos + i]) {
            i++;
            if (p->pos + i >= p->size)
                break;
        }
    }
    return key[i] == 0;
}

/* Moves the cursor to the first place where the key starts, then past the key. Returns 1 when
 * the key is in the text. When it is not, the cursor goes to the end of the text plus the
 * length of the key. */
int findstring(const char *key)
{
    Parser *p = &g_parser;
    int size = p->size;
    int i = p->pos;

    while (i < size) {
        p->pos = i;
        if (checkstring(key))
            break;
        i++;
    }
    p->pos = i + (int)strlen(key);
    return p->pos < size;
}

static int is_number_char(char c)
{
    return (c >= '0' && c <= '9') || c == '-';
}

/* Skips to the next digit or minus sign, then reads up to 255 of them as a number. A text with
 * no number gives 0. */
int getint(void)
{
    Parser *p = &g_parser;
    int n = 0;
    int value = 0;

    while (p->pos < p->size && !is_number_char(p->text[p->pos]))
        p->pos++;
    while (p->pos < p->size && n < 0xff && is_number_char(p->text[p->pos])) {
        p->number[n++] = p->text[p->pos];
        p->pos++;
    }
    p->number[n] = 0;
    os_sscanf(p->number, "%d", &value);
    return value;
}

/* Copies the text from the cursor to the end of the line, at most max - 1 bytes. The cursor
 * stays on the byte that ends the copy. The caller makes sure that the cursor is inside the
 * text. Returns the number of bytes copied. */
static int copy_line(Parser *p, char *out, int max)
{
    int i = 0;
    char c;

    do {
        if (i >= max - 1)
            break;
        c = p->text[p->pos];
        if (c == '\r' || c == '\n')
            break;
        out[i++] = c;
        p->pos++;
    } while (p->pos < p->size);
    return i;
}

/* Reads the rest of the line into out. The cursor stays at the end of the line. */
void getstring(char *out, int max)
{
    Parser *p = &g_parser;

    if (p->pos >= p->size) {
        out[0] = 0;
        return;
    }
    out[copy_line(p, out, max)] = 0;
}

/* Reads a line into out and moves the cursor to the start of the next line. */
void cs_getline(char *out, int max)
{
    Parser *p = &g_parser;
    int i = 0;

    if (p->pos < p->size)
        i = copy_line(p, out, max);
    out[i] = 0;
    while (p->pos < p->size && (p->text[p->pos] == '\r' || p->text[p->pos] == '\n'))
        p->pos++;
}
