/* One key of the settings file.
 *
 * A read sets the value from the text of the file. A write puts the key and the value on one line
 * of the open file. The open file is the shared file handle of the client.
 */
#include "setup/option_keys.h"

#include <stdint.h>

#include "client.h"
#include "options.h"
#include "sr_runtime.h"

/* Set an int from the number after the key, or write the key and the int. */
void __cdecl optionreadwriteint(int *value, const char *key, int write)
{
    option_int(value, key, write, (void *)(uintptr_t)cl_data_open_file);
}

/* Set a string from the rest of the line after the key, or write the key and the string. */
void __cdecl optionreadwritestring(char *value, const char *key, int max, int write)
{
    option_string(value, key, max, write, (void *)(uintptr_t)cl_data_open_file);
}
