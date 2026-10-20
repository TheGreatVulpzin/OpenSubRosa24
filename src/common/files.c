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

#define g_filelist (*SR_AT(FileList, SR_RVA_FILELIST))

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

/* A name that does not fit in 64 bytes runs into the next name. The list has room for 1024
 * names: the 0 after the last name of a full list goes to the low byte of the count of the
 * next list. */
void listfiles(const char *pattern, int list, int dirs)
{
    FileListSlot *l = &g_filelist.list[list];
    struct _finddata64i32_t fd;
    intptr_t handle, found;

    handle = os_findfirst64i32(pattern, &fd);
    l->count = 0;
    found = handle;
    for (;;) {
        if (found == -1 || l->count > MAX_NAMES - 1)
            break;
        if (!dirs) {
            if (!(fd.attrib & _A_SUBDIR)) {
                strcpy(l->name[l->count], fd.name);
                l->count++;
            }
        } else if ((fd.attrib & _A_SUBDIR) && fd.name[0] != '.') {
            strcpy(l->name[l->count], fd.name);
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
