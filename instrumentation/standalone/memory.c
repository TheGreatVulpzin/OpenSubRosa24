/* Image storage for differential recordings only. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "image_024d.h"

uintptr_t sr_base;
void sv_log(const char *fmt, ...);
#ifdef SR_RECORD
void sr_fn_init(void);
#endif

uint32_t sv_record_memory_base(void)
{
    return (uint32_t)sr_base;
}

void sv_record_log_memory_address(uintptr_t address)
{
    if (address >= sr_base && address < sr_base + sr_image_size)
        sv_log("CRASH: the data address is image RVA 0x%x", (unsigned)(address - sr_base));
}

int sv_record_memory_init(void)
{
    unsigned i;
    uint8_t *base = VirtualAlloc((void *)0x20000000, sr_image_size,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) {
        fprintf(stderr, "cannot allocate recording memory (%lu)\n", GetLastError());
        return 0;
    }
    sr_base = (uintptr_t)base;
    memcpy(base + sr_image_rdata_rva, sr_image_rdata, sr_image_rdata_size);
    memcpy(base + sr_image_data_rva, sr_image_data, sr_image_data_size);
    for (i = 0; i < sr_image_reloc_count; ++i)
        *(uint32_t *)(base + sr_image_relocs[i]) += (uint32_t)(sr_base - 0x400000u);
#ifdef SR_RECORD
    sr_fn_init();
#endif
    return 1;
}
