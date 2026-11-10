/* Debug output of the client: crosses that mark a point, named values that the debug display
 * shows, and the debug log file. All of them do nothing when the debug flag is off, except
 * debug_addcross, which has no flag.
 */
#include "debug/debug.h"

#include <string.h>

#include "sr_os.h"
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

#define g_debug sr_data_debug
#define g_debugit sr_data_debugit

/* A cross makes 3 lines. The array has room for 4096 lines. */
#define MAX_CROSS_COUNT 0xffd

/* The list of named values. Integer and float values share it. */
#define MAX_INT_VALUES 0x80
#define MAX_FLOAT_VALUES 0x20

#define SEEK_END_OF_FILE 2

/* Add a cross at center: one line along each axis, each size long on both sides. Nothing is
 * added when the array is nearly full. */
void __cdecl debug_addcross(const vec3 *center, float size)
{
    Debug *d = &g_debug;
    DebugLine *line;

    if (d->num_lines >= MAX_CROSS_COUNT)
        return;
    line = &d->lines[d->num_lines++];
    line->from.x = (float)(center->x - size);
    line->from.y = center->y;
    line->from.z = center->z;
    line->to.x = (float)(center->x + size);
    line->to.y = center->y;
    line->to.z = center->z;
    line = &d->lines[d->num_lines++];
    line->from.x = center->x;
    line->from.y = (float)(center->y - size);
    line->from.z = center->z;
    line->to.x = center->x;
    line->to.y = (float)(center->y + size);
    line->to.z = center->z;
    line = &d->lines[d->num_lines++];
    line->from.x = center->x;
    line->from.y = center->y;
    line->from.z = (float)(center->z - size);
    line->to.x = center->x;
    line->to.y = center->y;
    line->to.z = (float)(center->z + size);
}

/* Find the value with this name, or add it when the list has room (the last name that
 * matches wins). The result is its index, or -1 when the list is full. */
static int find_value(const char *name, int limit)
{
    Debug *d = &g_debug;
    int found = -1;
    int i;

    for (i = 0; i < d->num_values; i++) {
        if (strcmp(name, d->values[i].name) == 0)
            found = i;
    }
    if (found == -1) {
        if (d->num_values > limit - 1)
            return -1;
        found = d->num_values++;
        strcpy(d->values[found].name, name);
    }
    return found;
}

/* Show an integer under a name. */
void __cdecl debug_addataint(const char *name, int value)
{
    int index;

    if (g_debugit == 0)
        return;
    index = find_value(name, MAX_INT_VALUES);
    if (index < 0)
        return;
    g_debug.values[index].is_float = 0;
    g_debug.values[index].int_value = value;
}

/* Show a float under a name. A new name needs room in the first 32 entries. */
void __cdecl debug_addatafloat(const char *name, float value)
{
    int index;

    if (g_debugit == 0)
        return;
    index = find_value(name, MAX_FLOAT_VALUES);
    if (index < 0)
        return;
    g_debug.values[index].is_float = 1;
    g_debug.values[index].float_value = value;
}

/* Add a line of text at the end of the debug log file. The file must exist. */
void __cdecl debug_writelogfile(const char *text)
{
    static int(__cdecl * seek)(void *, long, int);
    Debug *d = &g_debug;

    if (g_debugit == 0)
        return;
    d->log_file = os_fopen(d->log_path, "rb+");
    if (d->log_file == 0)
        return;
    if (!seek)
        *(void **)&seek = sr_import("MSVCR100.DLL", "fseek");
    seek(d->log_file, 0, SEEK_END_OF_FILE);
    os_fprintf(d->log_file, "%s\r\n", text);
    os_fclose(d->log_file);
}
