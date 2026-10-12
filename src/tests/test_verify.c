/* Teste do verificador genérico (hook/verify.c) sem o jogo.
 * As "globais do jogo" ficam na seção .gdata deste executável. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "sr_runtime.h"
#include "verify.h"

#pragma section(".gdata", read, write)
__declspec(allocate(".gdata")) static volatile int g_vals[4096];

static int failures;

#define CHECK(cond, msg)                                   \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FALHOU: %s (linha %d)\n", msg, __LINE__); \
            failures++;                                    \
        } else {                                           \
            printf("ok: %s\n", msg);                       \
        }                                                  \
    } while (0)

static void run(SvVerify *v, void (*new_fn)(void), void (*orig_fn)(void))
{
    if (!sv_verify_should(v)) {
        new_fn();
        return;
    }
    sv_track_begin_new(v);
    new_fn();
    sv_track_switch_to_original();
    orig_fn();
    sv_verify_end(v, 1, "teste");
}

static void write_a(void) { g_vals[10] = 7; g_vals[2000] = 9; }
static void write_a_copy(void) { g_vals[10] = 7; g_vals[2000] = 9; }
static void write_b(void) { g_vals[10] = 8; g_vals[2000] = 9; }
static void write_c_only(void) { g_vals[3000] = 5; }
static void nothing(void) { }

int main(void)
{
    SvVerify v1 = { "iguais", 1 };
    SvVerify v2 = { "valor_diferente", 1 };
    SvVerify v3 = { "so_original_escreve", 1 };
    SvVerify v4 = { "amostragem", 3 };
    int i;

    sr_base = (uintptr_t)GetModuleHandleA(NULL);
    sv_verify_set_section(".gdata");
    CHECK(sv_verify_init(), "init");

    run(&v1, write_a, write_a_copy);
    CHECK(v1.checks == 1 && v1.fails == 0, "versões iguais: 0 diferenças");
    CHECK(g_vals[10] == 7 && g_vals[2000] == 9, "versões iguais: estado final correto");

    g_vals[10] = 0;
    run(&v2, write_b, write_a);
    CHECK(v2.fails == 1, "valor diferente: detectado");
    CHECK(g_vals[10] == 7, "valor diferente: estado final é o da original");

    run(&v3, nothing, write_c_only);
    CHECK(v3.fails == 1, "escrita só da original: detectada");
    CHECK(g_vals[3000] == 5, "escrita só da original: estado final é o da original");

    for (i = 0; i < 7; i++)
        run(&v4, write_a, write_a_copy);
    CHECK(v4.calls == 7 && v4.checks == 3, "amostragem 1 a cada 3: 3 de 7 verificadas");

    {
        SvVerify v5 = { "escopo", 1 };
        for (i = 0; i < 5; i++)
            run(&v5, write_a, write_a_copy);
        CHECK(v5.nranges > 0, "escopo aprendido depois das rodadas completas");
        run(&v5, write_b, write_a);
        CHECK(v5.fails == 1, "diferença detectada numa rodada com escopo");
    }

    g_vals[1] = 1;  /* a seção voltou a aceitar escrita */
    CHECK(g_vals[1] == 1, "seção liberada depois da verificação");

    printf(failures ? "\n%d FALHA(S)\n" : "\nTODOS OS TESTES PASSARAM\n", failures);
    return failures != 0;
}
