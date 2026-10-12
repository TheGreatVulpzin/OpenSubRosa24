/* Tabela de hooks do subrosadedicated024d.exe.
 *
 * Modo de verificação (subvulpes.ini: verify=1, o padrão nesta fase):
 * a cada chamada, roda a função nova, guarda o resultado, desfaz,
 * roda a original pelo trampolim e compara a memória. Diferença vai para o log.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

#include "hook.h"
#include "sr_runtime.h"
#include "sr_server.h"

static int g_verify;
static int g_log_calls;   /* subvulpes.ini: log_calls=1 registra cada chamada verificada */

typedef struct VerifyStats {
    const char *name;
    long checks;
    long fails;
} VerifyStats;

static void verify_result(VerifyStats *s, int ok, const char *detail)
{
    s->checks++;
    if (g_log_calls)
        sv_log("chamada %s #%ld: %s \"%s\"", s->name, s->checks, ok ? "igual" : "DIFERENTE", detail);
    if (!ok) {
        s->fails++;
        if (s->fails <= 20)
            sv_log("VERIFICA %s: DIFERENTE (chamada %ld) %s", s->name, s->checks, detail);
    }
    if (s->checks == 1 || s->checks % 1000 == 0)
        sv_log("VERIFICA %s: %ld chamadas, %ld diferenças", s->name, s->checks, s->fails);
}

/* ------------------------------------------------------------ createevent_message */

typedef void(__cdecl *createevent_message_fn)(int, const char *, int, int);
static createevent_message_fn orig_createevent_message;
static VerifyStats st_createevent_message = { "createevent_message", 0, 0 };

static void __cdecl hk_createevent_message(int kind, const char *text, int target, int extra)
{
    uint32_t idx, n_before, n_mine;
    Event before, mine;

    if (!g_verify) {
        sr_createevent_message(kind, text, target, extra);
        return;
    }

    idx = sr_numofevents & 0xffff;
    n_before = sr_numofevents;
    before = sr_events[idx];

    sr_createevent_message(kind, text, target, extra);
    mine = sr_events[idx];
    n_mine = sr_numofevents;

    sr_events[idx] = before;
    sr_numofevents = n_before;
    orig_createevent_message(kind, text, target, extra);

    verify_result(&st_createevent_message,
                  memcmp(&mine, &sr_events[idx], sizeof(Event)) == 0 && n_mine == sr_numofevents,
                  text);
}

/* ------------------------------------------------------------ tabela */

/* cmp dword ptr [server], 0  ->  83 3D <endereço de server> 00 */
static uint8_t expect_createevent_message[7] = { 0x83, 0x3D, 0, 0, 0, 0, 0x00 };
static const uint8_t mask_all[16] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static SvHook g_hooks[] = {
    { "createevent_message", 0x6260, (void *)hk_createevent_message, 7,
      expect_createevent_message, mask_all, (void **)&orig_createevent_message, 0, 0 },
};

int sv_hooks_024d_install(void)
{
    /* O endereço absoluto de "server" dentro da instrução muda com o ASLR. */
    uint32_t server_va = (uint32_t)(sr_base + SR_RVA_SERVER);
    memcpy(&expect_createevent_message[2], &server_va, 4);

    g_verify = sv_config_int("verify", 1);
    g_log_calls = sv_config_int("log_calls", 1);
    sv_log("modo de verificação: %s", g_verify ? "ligado" : "desligado");
    return sv_install_hooks(g_hooks, (int)(sizeof g_hooks / sizeof g_hooks[0]));
}

/* Chamado quando o processo fecha: resumo final da verificação. */
void sv_hooks_024d_summary(void)
{
    if (g_verify)
        sv_log("RESUMO %s: %ld chamadas, %ld diferenças", st_createevent_message.name,
               st_createevent_message.checks, st_createevent_message.fails);
}
