/* The harness recording of the standalone server (subvulpes-server-record.exe).
 *
 * The recording build compiles the lifted code (src/lifted/lifted_024d.c) with /Gh: each
 * function calls _penter at its start. _penter finds the function in lf_table and gives its
 * arguments to the recorder of the hook build (instrumentation/hook/record.c):
 *   - a pure function: sv_record_args, one "R" line with the arguments;
 *   - a side-effect function (lf_table side=1): sv_rec_begin at the start, and sv_rec_end when
 *     it returns, with one "E" line for each import call (the wrappers of
 *     instrumentation/hook/record_imports.c, which sr_import gives to the game in this build). _penter puts
 *     rec_return in place of the return address of the function, to see its return.
 * src/game is not in the recording: its physics functions have many recorded calls, and the
 * hottest functions are there.
 * The image region is at 0x20000000, as the fixed-base 024d of the hook recordings
 * (tools/re/fixed_base.py), so the harness reads these recordings with no change.
 *
 * The output goes to SV_RECORD_DIR: sv_args.txt and sv_snapshot.bin (SV_SNAPSHOT_AFTER
 * seconds after the start, default 600). record_focus.txt next to the exe chooses the
 * functions to record most (instrumentation/hook/record.c).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lifted_table.h"
#include "record.h"

extern uintptr_t sr_base;
void sv_log(const char *fmt, ...);
extern uint32_t (*sv_record_code)(uint32_t v);

#define RVA_SERVERCYCLE 0x33570u  /* runs once each tick: the snapshot timer */
#define RVA_DATA 0x60000u         /* .data of the 024d */
#define DATA_SIZE 0x25e9a000u

/* ------------------------------------------------------------ the hook API of record.c */

/* subvulpes.ini keys of the hook build. Here SV_<KEY> in the environment, or the default
 * of a recording session. */
int sv_config_int(const char *key, int def)
{
    char name[64] = "SV_";
    const char *v;
    size_t i;
    for (i = 0; key[i] && i < sizeof name - 4; i++)
        name[3 + i] = (char)toupper((unsigned char)key[i]);
    name[3 + i] = 0;
    if ((v = getenv(name)) != NULL)
        return atoi(v);
    if (!strcmp(key, "record_args") || !strcmp(key, "record_imports") || !strcmp(key, "original_only"))
        return 1;
    if (!strcmp(key, "snapshot_after"))
        return 600;
    return def;
}

int sv_original_only(void) { return 1; }
int sv_verify_busy(void) { return 0; }
FARPROC *sv_wsock_slot(const char *name) { (void)name; return NULL; }

void sv_console_event(int error, const char *fmt, ...)
{
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    sv_log("%s%s", error ? "ERROR " : "", text);
}

int sv_verify_data(const uint8_t **data, size_t *size)
{
    *data = (const uint8_t *)(sr_base + RVA_DATA);
    *size = DATA_SIZE;
    return 1;
}

/* A "hook" here is the record flag of one lf_table entry. record.c removes the hooks of the
 * functions that have their calls and puts some back in a rotation. */
static uint8_t *g_off;
static int g_unhooked;

static int entry_of(uint32_t rva)
{
    int i;
    for (i = 0; i < lf_table_count; i++)
        if (lf_table[i].rva == rva)
            return i;
    return -1;
}

int sv_unhook_rva(uint32_t rva)
{
    int i = entry_of(rva);
    if (i < 0 || g_off[i])
        return 0;
    g_off[i] = 1;
    g_unhooked++;
    return 1;
}

int sv_rehook_rva(uint32_t rva)
{
    int i = entry_of(rva);
    if (i < 0 || !g_off[i])
        return 0;
    g_off[i] = 0;
    g_unhooked--;
    return 1;
}

int sv_unhooked_count(void) { return g_unhooked; }

/* ------------------------------------------------------------ function addresses */

/* Open addressing: the address of each lf_ function to its lf_table index. */
#define SLOTS 2048
static uint32_t g_addr[SLOTS];
static int16_t g_index[SLOTS];

static unsigned slot_of(uint32_t a)
{
    return (a * 2654435761u >> 21) & (SLOTS - 1);
}

static int lookup(uint32_t a)
{
    unsigned s = slot_of(a);
    while (g_addr[s]) {
        if (g_addr[s] == a)
            return g_index[s];
        s = (s + 1) & (SLOTS - 1);
    }
    return -1;
}

/* A C function of the game in a value: the 024d address of the same function. */
static uint32_t code_value(uint32_t v)
{
    int i = v >= 0x10000 ? lookup(v) : -1;
    return i < 0 ? v : (uint32_t)sr_base + lf_table[i].rva;
}

/* ------------------------------------------------------------ _penter */

/* The real return addresses of the open side-effect calls. */
#define DEPTH 256
static uint32_t g_ret[DEPTH];
static int g_depth;

static void rec_return(void);

/* fn: the start of the function (the return address of its call to _penter, minus 5).
 * frame[0]: the return address of the function, frame[1...]: its arguments. */
static void __cdecl on_enter(uint32_t fn, uint32_t *frame)
{
    int i = lookup(fn);
    const LfEntry *e;
    if (i < 0)
        return;
    e = &lf_table[i];
    if (e->rva == RVA_SERVERCYCLE)
        sv_snapshot_check();
    if (g_off[i])
        return;
    if (!e->side) {
        sv_record_args(e->rva, i, frame + 1, e->dwords);
        return;
    }
    if (g_depth == DEPTH)
        return;
    sv_rec_begin(e->rva, frame + 1, e->dwords);
    sv_rec_push_ret(frame[0]);
    g_ret[g_depth++] = frame[0];
    frame[0] = (uint32_t)(uintptr_t)rec_return;
}

/* A side-effect function returns here: close its record, then go to its real return address. */
static uint32_t __cdecl on_return(uint32_t eax)
{
    sv_rec_end(eax);
    return g_ret[--g_depth];
}

/* The registers and the flags of the game function stay the same. No FXSAVE: _penter runs
 * millions of times each second, and FXSAVE with FXRSTOR took the server from 62 to 21 ticks
 * per second. At the start of a function the x87 stack is empty and the SSE registers hold
 * nothing (both are free across a call). At rec_return, ST0 can hold a float result: the
 * recorder uses no float math, so the x87 stack stays as it is. */
__declspec(naked) void __cdecl _penter(void)
{
    __asm {
        pushad
        pushfd
        lea eax, [esp + 40]
        push eax
        mov eax, [esp + 40]
        sub eax, 5
        push eax
        call on_enter
        add esp, 8
        popfd
        popad
        ret
    }
}

__declspec(naked) static void rec_return(void)
{
    __asm {
        push eax          /* the place of the real return address */
        pushad
        pushfd
        push eax
        call on_return
        add esp, 4
        mov [esp + 36], eax
        popfd
        popad
        ret
    }
}

/* ------------------------------------------------------------ start */

void sv_record_start(void *vprintf_fn)
{
    int i;
    g_off = calloc((size_t)lf_table_count, 1);
    for (i = 0; i < lf_table_count; i++) {
        uint32_t a = (uint32_t)(uintptr_t)lf_table[i].lifted;
        unsigned s = slot_of(a);
        while (g_addr[s])
            s = (s + 1) & (SLOTS - 1);
        g_addr[s] = a;
        g_index[s] = (int16_t)i;
    }
    sv_record_code = code_value;
    sv_record_imports_printf((void *)vfprintf, vprintf_fn, (void *)_vsnprintf);
    sv_log("harness recording: %d functions, output in %s", lf_table_count,
           getenv("SV_RECORD_DIR") ? getenv("SV_RECORD_DIR") : "the exe folder");
}
