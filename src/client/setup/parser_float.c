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

