/* The float reader of the text parser. */
#include "setup/parser_float.h"

#include "sr_os.h"
#include "sr_runtime.h"

#define g_parser sr_data_parser
#define NUMBER_LIMIT 0xff

/* Tell if a character can be part of a number: a digit, a minus sign or a point. */
static int is_float_char(char c)
{
    return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

/* Skip to the next character of a number, then read up to 255 of them as a float. A text with no number
 * gives 0. */
float __cdecl getfloat(void)
{
    Parser *p = &g_parser;
    int n = 0;
    float value = 0.0f;

    while (p->pos < p->size && !is_float_char(p->text[p->pos]))
        p->pos++;
    while (p->pos < p->size && n < NUMBER_LIMIT && is_float_char(p->text[p->pos])) {
        p->number[n++] = p->text[p->pos];
        p->pos++;
    }
    p->number[n] = 0;
    os_sscanf(p->number, "%f", &value);
    return value;
}
