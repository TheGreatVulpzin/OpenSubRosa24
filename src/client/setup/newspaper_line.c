/* The lines of the newspaper text. */
#include "setup/newspaper_line.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define NEWSPAPER_LINES 0x400
#define NEWSPAPER_PART 0x40
#define NEWSPAPER_SECOND_PART 0x3f
#define NEWSPAPER_END_MARK 0x7c

/* Store 64 bytes of text in a line of the newspaper. With mode 0 they go to the start of the line.
 * With another mode they go to byte 63 of the line, and then byte 124 of the line is set to 0. The
 * count of lines grows to include the line, and the texture of the newspaper must be made again.
 * A line number outside 0 to 1023 changes nothing. */
void __cdecl newspaper_addline(const void *text, int line, int mode)
{
    if (line < 0 || line >= NEWSPAPER_LINES)
        return;
    if (mode == 0) {
        memcpy(sr_data_newspaper.lines[line], text, NEWSPAPER_PART);
    } else {
        memcpy(sr_data_newspaper.lines[line] + NEWSPAPER_SECOND_PART, text, NEWSPAPER_PART);
        sr_data_newspaper.lines[line][NEWSPAPER_END_MARK] = 0;
    }
    if (sr_data_newspaper.num_lines < line + 1)
        sr_data_newspaper.num_lines = line + 1;
    sr_data_newspaper.texture_done = 0;
}
