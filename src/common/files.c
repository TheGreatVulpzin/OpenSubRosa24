/* The file name lists of the game.
 *
 * The game keeps 8 lists in one global. A list holds up to 1024 names of 64 bytes, and the
 * name bytes follow the count. listfiles fills a list from a folder search, and
 * removeflieextentions cuts the names at the dot.
 */
#include "files.h"

#include <io.h>
#include <stdlib.h>
#include <string.h>

#include "sr_os.h"
#include "sr_runtime.h"

#define NAME_SIZE 64
#define MAX_NAMES 1024

#define g_filelist sr_data_filelist

int __cdecl comparestrings(const void *a, const void *b)
{
    const unsigned char *p = a, *q = b;
    for (;;) {
        if (*p != *q)
            return *p < *q ? -1 : 1;
        if (*p == 0)
            return 0;
        p++;
        q++;
    }
}

/* Reserves one name slot for the end marker. Each name has a final 0. */
void listfiles(const char *pattern, int list, int dirs)
{
    FileListSlot *l = &g_filelist.list[list];
    struct _finddata64i32_t fd;
    intptr_t handle, found;
    size_t length;

    handle = os_findfirst64i32(pattern, &fd);
    l->count = 0;
    found = handle;
    for (;;) {
        if (found == -1 || l->count >= MAX_NAMES - 1)
            break;
        if (!dirs) {
            if (!(fd.attrib & _A_SUBDIR)) {
                length = strnlen(fd.name, NAME_SIZE - 1);
                strncpy(l->name[l->count], fd.name, length);
                l->name[l->count][length] = 0;
                l->count++;
            }
        } else if ((fd.attrib & _A_SUBDIR) && fd.name[0] != '.') {
            length = strnlen(fd.name, NAME_SIZE - 1);
            strncpy(l->name[l->count], fd.name, length);
            l->name[l->count][length] = 0;
            l->count++;
        }
        found = os_findnext64i32(handle, &fd);
    }
    ((char *)l->name)[l->count * NAME_SIZE] = 0;
    os_findclose(handle);
    qsort(l->name, l->count, NAME_SIZE, comparestrings);
}

/* A name with no dot and no 0 in 64 bytes loses the first byte of the next name. */
void removeflieextentions(int list)
{
    FileListSlot *l = &g_filelist.list[list];
    char *base = (char *)l->name;
    int i, j;

    for (i = 0; i < l->count; i++) {
        char *s = base + i * NAME_SIZE;
        for (j = 0; j < NAME_SIZE && s[j] != 0 && s[j] != '.'; j++)
            ;
        s[j] = 0;
    }
}
