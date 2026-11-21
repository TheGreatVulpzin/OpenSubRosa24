/* Text of command line arguments.
 * A quote can follow a backslash inside an argument.
 * Remove that backslash in place without changing the other bytes.
 */
#include "setup/arguments.h"

/* Remove a backslash before a quote. Continue at the next byte after each removal. */
void __cdecl unescapequotes(char *text)
{
    char *previous = 0;

    for (; *text; ++text) {
        if (*text == '"' && previous && *previous == '\\') {
            char *destination = previous;
            char *source;

            for (source = text; *source; ++source) {
                *destination = *source;
                destination = source;
            }
            *destination = '\0';
        }
        previous = text;
    }
}
