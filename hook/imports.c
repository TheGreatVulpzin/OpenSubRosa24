/* Resolve imports of the game through its own import table.
 *
 * Lifted code must call the same CRT as the game (MSVCR100.dll): rand(), FILE*,
 * malloc() and errno state live in that DLL. So lifted code never calls our own CRT
 * for these functions. It calls the address stored in the IAT of the game.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "hook.h"
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)sr_base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(sr_base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const IMAGE_IMPORT_DESCRIPTOR *imp = (const IMAGE_IMPORT_DESCRIPTOR *)(sr_base + dir->VirtualAddress);

    for (; imp->Name; imp++) {
        const IMAGE_THUNK_DATA32 *names, *slots;
        if (_stricmp((const char *)(sr_base + imp->Name), dll) != 0)
            continue;
        names = (const IMAGE_THUNK_DATA32 *)(sr_base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk
                                                                                  : imp->FirstThunk));
        slots = (const IMAGE_THUNK_DATA32 *)(sr_base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            const IMAGE_IMPORT_BY_NAME *by;
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
                continue;
            by = (const IMAGE_IMPORT_BY_NAME *)(sr_base + names->u1.AddressOfData);
            if (strcmp((const char *)by->Name, name) == 0)
                return (void *)(uintptr_t)slots->u1.Function;
        }
    }
    sv_log("ERROR: import %s!%s not found in the game", dll, name);
    return NULL;
}
