/* Hook table of subrosadedicated024d.exe.
 *
 * To add a function:
 *   1. Write sr_<name> in src/server/ and declare it in sr_server.h.
 *   2. Write the wrapper hk_<name> with SV_HOOK_VOID or SV_HOOK_RET (hook_wrap.h).
 *   3. Add a line to g_hooks with the RVA, the prologue size and the expected bytes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "hook.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "hook_wrap.h"
#include "verify.h"

/* ------------------------------------------------------------ createevent_message (0x6260) */

typedef void(__cdecl *createevent_message_fn)(int, const char *, int, int);
static createevent_message_fn orig_createevent_message;
static SvVerify v_createevent_message = { "createevent_message", -1 };

static void __cdecl hk_createevent_message(int kind, const char *text, int target, int extra)
{
    SV_HOOK_VOID(v_createevent_message,
                 sr_createevent_message(kind, text, target, extra),
                 orig_createevent_message(kind, text, target, extra),
                 text);
}

/* cmp dword ptr [server], 0  ->  83 3D <address of server> 00 */
static uint8_t expect_createevent_message[7] = { 0x83, 0x3D, 0, 0, 0, 0, 0x00 };

/* ------------------------------------------------------------ createevent_updatedoor (0x6660) */

typedef void(__cdecl *createevent_updatedoor_fn)(int, int);
static createevent_updatedoor_fn orig_createevent_updatedoor;
static SvVerify v_createevent_updatedoor = { "createevent_updatedoor", -1 };

static void __cdecl hk_createevent_updatedoor(int a, int b)
{
    SV_HOOK_VOID(v_createevent_updatedoor, sr_createevent_updatedoor(a, b),
                 orig_createevent_updatedoor(a, b), NULL);
}

static uint8_t expect_createevent_updatedoor[7] = { 0x83, 0x3D, 0, 0, 0, 0, 0x00 };

/* ------------------------------------------------------------ createevent_updatemission (0x6760) */

typedef void(__cdecl *createevent_updatemission_fn)(int, int);
static createevent_updatemission_fn orig_createevent_updatemission;
static SvVerify v_createevent_updatemission = { "createevent_updatemission", -1 };

static void __cdecl hk_createevent_updatemission(int team, int mission)
{
    SV_HOOK_VOID(v_createevent_updatemission, sr_createevent_updatemission(team, mission),
                 orig_createevent_updatemission(team, mission), NULL);
}

static uint8_t expect_createevent_updatemission[7] = { 0x83, 0x3D, 0, 0, 0, 0, 0x00 };

/* ------------------------------------------------------------ fixplayername (0xa580) */

typedef void(__cdecl *fixplayername_fn)(int);
static fixplayername_fn orig_fixplayername;
static SvVerify v_fixplayername = { "fixplayername", -1 };

static void __cdecl hk_fixplayername(int id)
{
    SV_HOOK_VOID(v_fixplayername, sr_fixplayername(id), orig_fixplayername(id), sr_players[id].name);
}

/* push esi; mov esi, [esp+8] */
static const uint8_t expect_fixplayername[5] = { 0x56, 0x8B, 0x74, 0x24, 0x08 };

/* ------------------------------------------------------------ table */

static const uint8_t mask_all[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static SvHook g_hooks[] = {
    { "createevent_message", 0x6260, (void *)hk_createevent_message, 7,
      expect_createevent_message, mask_all, (void **)&orig_createevent_message, 0, 0 },
    { "createevent_updatedoor", 0x6660, (void *)hk_createevent_updatedoor, 7,
      expect_createevent_updatedoor, mask_all, (void **)&orig_createevent_updatedoor, 0, 0 },
    { "createevent_updatemission", 0x6760, (void *)hk_createevent_updatemission, 7,
      expect_createevent_updatemission, mask_all, (void **)&orig_createevent_updatemission, 0, 0 },
    { "fixplayername", 0xa580, (void *)hk_fixplayername, 5,
      expect_fixplayername, mask_all, (void **)&orig_fixplayername, 0, 0 },
};

static SvVerify *g_stats[] = {
    &v_createevent_message,
    &v_createevent_updatedoor,
    &v_createevent_updatemission,
    &v_fixplayername,
};

/* Write an absolute address (relocated by ASLR) into the expected bytes. */
static void expect_va(uint8_t *expect, int offset, uint32_t rva)
{
    uint32_t va = (uint32_t)(sr_base + rva);
    memcpy(expect + offset, &va, 4);
}

int sv_hooks_lifted_install(void);
void sv_hooks_lifted_summary(void);

int sv_hooks_024d_install(void)
{
    int n;
    expect_va(expect_createevent_message, 2, SR_RVA_SERVER);
    expect_va(expect_createevent_updatedoor, 2, SR_RVA_SERVER);
    expect_va(expect_createevent_updatemission, 2, SR_RVA_SERVER);

    sv_verify_init();
    /* timer[256] (0x58 bytes each, size from the Linux ELF symbol): profiler start and
     * total times read from the CPU cycle counter (RDTSC). Each run writes other values. */
    sv_verify_ignore(0x14043e0, 0x5800);
    n = sv_install_hooks(g_hooks, (int)(sizeof g_hooks / sizeof g_hooks[0]));
    /* Functions lifted automatically from Ghidra (tools/lift/lift.py). subvulpes.ini: lifted=0 disables all. */
    if (sv_config_int("lifted", 1))
        n += sv_hooks_lifted_install();
    else
        sv_log("lifted hooks disabled (lifted=0)");
    return n;
}

/* Called when the process exits. */
void sv_hooks_024d_summary(void)
{
    size_t i;
    for (i = 0; i < sizeof g_stats / sizeof g_stats[0]; i++)
        if (g_stats[i]->calls)
            sv_verify_summary(g_stats[i]);
    sv_hooks_lifted_summary();
}
