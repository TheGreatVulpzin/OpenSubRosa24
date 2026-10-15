/* SubVulpes hook system.
 *
 * Each hook replaces the start of a 024d function with a JMP to the C version.
 * The trampoline lets us call the original function (for the verification mode).
 */
#ifndef SV_HOOK_H
#define SV_HOOK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SvHook {
    const char *name;           /* function name (same as the 0.24c ELF) */
    uint32_t rva;               /* function start in the 024d */
    void *replacement;          /* new function */
    uint8_t prologue_len;       /* bytes copied to the trampoline (>= 5, whole instructions) */
    const uint8_t *expect;      /* expected original bytes at the function start */
    const uint8_t *mask;        /* 0xff = check the byte; 0x00 = ignore (relocated address) */
    void **original;            /* output: pointer to call the original through the trampoline */
    int enabled;                /* read from subvulpes.ini */
    int installed;
} SvHook;

/* Log to subvulpes.log, in the folder of the executable. */
void sv_log(const char *fmt, ...);

/* Remove one installed hook (hook.c). Game thread only. */
int sv_unhook_rva(uint32_t rva);
int sv_rehook_rva(uint32_t rva);
int sv_unhooked_count(void);

/* Server console (console.c): banner, window title and [SubVulpes] event lines. */
void sv_console_init(const char *mode, int hooks);
void sv_console_event(int error, const char *fmt, ...);

/* Recording size so far (record.c): calls written to sv_args.txt and its size in bytes. */
void sv_record_stats(int *calls, long long *bytes);

/* Read subvulpes.ini. Returns the value of the key, or def if the key does not exist. */
int sv_config_int(const char *key, int def);

/* Install all hooks of the table. Returns the number of installed hooks. */
int sv_install_hooks(SvHook *hooks, int count);

#ifdef __cplusplus
}
#endif

#endif /* SV_HOOK_H */
