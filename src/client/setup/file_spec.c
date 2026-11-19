/* The match of a file name against a spec with the wildcard *. */
#include "setup/file_spec.h"

#include <string.h>

/* Tell if a name fits a spec. The spec and the name are read together. A star in the spec lets the
 * name go on until it has the character after the star. The result is 1 when the spec or the name
 * ends, and 0 for a character that differs and is not covered by a star. */
int __cdecl checkfilespec(const char *spec, const char *name)
{
    int s = 0, n = 0;

    (void)strlen(name);
    for (;;) {
        if (spec[s] == '\0' || name[n] == '\0')
            return 1;
        if (spec[s] != name[n] && spec[s] != '*')
            return 0;
        if (spec[s] == name[n])
            s++;
        else if (spec[s] == '*' && spec[s + 1] == name[n])
            s += 2;
        n++;
    }
}
