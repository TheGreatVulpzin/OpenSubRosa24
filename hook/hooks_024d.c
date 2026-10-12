/* Tabela de hooks do subrosadedicated024d.exe.
 *
 * Para acrescentar uma função:
 *   1. Escreva sr_<nome> em src/server/ e declare em sr_server.h.
 *   2. Crie o wrapper hk_<nome> com SV_HOOK_VOID ou SV_HOOK_RET.
 *   3. Acrescente uma linha em g_hooks com o RVA, o tamanho do prólogo e os bytes esperados.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "hook.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "verify.h"

/* Wrapper de função void. NEW e ORIG são as chamadas completas, com argumentos. */
#define SV_HOOK_VOID(STAT, NEW, ORIG, DETAIL) \
    do {                                      \
        if (!sv_verify_should(&STAT)) {       \
            NEW;                              \
            return;                           \
        }                                     \
        sv_track_begin_new();                 \
        NEW;                                  \
        sv_track_switch_to_original();        \
        ORIG;                                 \
        sv_verify_end(&STAT, 1, DETAIL);      \
    } while (0)

/* Wrapper de função com retorno. Compara os retornos e devolve o da original. */
#define SV_HOOK_RET(TYPE, STAT, NEW, ORIG, DETAIL)            \
    do {                                                      \
        TYPE r_new_, r_orig_;                                 \
        if (!sv_verify_should(&STAT))                         \
            return NEW;                                       \
        sv_track_begin_new();                                 \
        r_new_ = NEW;                                         \
        sv_track_switch_to_original();                        \
        r_orig_ = ORIG;                                       \
        sv_verify_end(&STAT, r_new_ == r_orig_, DETAIL);      \
        return r_orig_;                                       \
    } while (0)

/* ------------------------------------------------------------ createevent_message (0x6260) */

typedef void(__cdecl *createevent_message_fn)(int, const char *, int, int);
static createevent_message_fn orig_createevent_message;
static SvVerify v_createevent_message = { "createevent_message", -1, 0, 0, 0 };

static void __cdecl hk_createevent_message(int kind, const char *text, int target, int extra)
{
    SV_HOOK_VOID(v_createevent_message,
                 sr_createevent_message(kind, text, target, extra),
                 orig_createevent_message(kind, text, target, extra),
                 text);
}

/* cmp dword ptr [server], 0  ->  83 3D <endereço de server> 00 */
static uint8_t expect_createevent_message[7] = { 0x83, 0x3D, 0, 0, 0, 0, 0x00 };

/* ------------------------------------------------------------ tabela */

static const uint8_t mask_all[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static SvHook g_hooks[] = {
    { "createevent_message", 0x6260, (void *)hk_createevent_message, 7,
      expect_createevent_message, mask_all, (void **)&orig_createevent_message, 0, 0 },
};

static SvVerify *g_stats[] = {
    &v_createevent_message,
};

/* Escreve um endereço absoluto (relocado pelo ASLR) dentro dos bytes esperados. */
static void expect_va(uint8_t *expect, int offset, uint32_t rva)
{
    uint32_t va = (uint32_t)(sr_base + rva);
    memcpy(expect + offset, &va, 4);
}

int sv_hooks_024d_install(void)
{
    expect_va(expect_createevent_message, 2, SR_RVA_SERVER);

    sv_verify_init();
    return sv_install_hooks(g_hooks, (int)(sizeof g_hooks / sizeof g_hooks[0]));
}

/* Chamado quando o processo fecha. */
void sv_hooks_024d_summary(void)
{
    size_t i;
    for (i = 0; i < sizeof g_stats / sizeof g_stats[0]; i++)
        if (g_stats[i]->calls)
            sv_verify_summary(g_stats[i]);
}
