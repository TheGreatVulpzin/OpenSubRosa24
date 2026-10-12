/* Test of the generic verifier (hook/verify.c) without the game.
 * The "game globals" live in the .gdata section of this executable. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "sr_runtime.h"
#include "hook_wrap.h"
#include "verify.h"

#pragma section(".gdata", read, write)
__declspec(allocate(".gdata")) static volatile int g_vals[4096];

static int failures;

#define CHECK(cond, msg)                                   \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAILED: %s (line %d)\n", msg, __LINE__); \
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
static void crash(void) { g_vals[20] = 3; *(volatile int *)0 = 1; }
static void write_d(void) { g_vals[20] = 4; }

static void hook_crash_test(SvVerify *v)
{
    SV_HOOK_VOID(*v, crash(), write_d(), "crash");
}

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
    CHECK(v1.checks == 1 && v1.fails == 0, "same versions: 0 differences");
    CHECK(g_vals[10] == 7 && g_vals[2000] == 9, "same versions: final state is correct");

    g_vals[10] = 0;
    run(&v2, write_b, write_a);
    CHECK(v2.fails == 1, "different value: detected");
    CHECK(g_vals[10] == 7, "different value: final state is the original one");

    run(&v3, nothing, write_c_only);
    CHECK(v3.fails == 1, "write only by the original: detected");
    CHECK(g_vals[3000] == 5, "write only by the original: final state is the original one");

    for (i = 0; i < 7; i++)
        run(&v4, write_a, write_a_copy);
    CHECK(v4.calls == 7 && v4.checks == 3, "sampling 1 of 3: 3 of 7 verified");

    {
        SvVerify v5 = { "escopo", 1 };
        for (i = 0; i < 5; i++)
            run(&v5, write_a, write_a_copy);
        CHECK(v5.nranges > 0, "scope learned after the full rounds");
        run(&v5, write_b, write_a);
        CHECK(v5.fails == 1, "difference detected in a scope round");
    }

    {
        SvVerify v6 = { "crash", 1 };
        hook_crash_test(&v6);
        CHECK(v6.broken == 1 && v6.fails == 1, "crash in the new version: hook marked as broken");
        CHECK(g_vals[20] == 4, "crash in the new version: its write undone, state of the original");
        g_vals[20] = 0;
        hook_crash_test(&v6);
        CHECK(g_vals[20] == 4 && v6.checks == 1, "broken hook: only the original runs");
    }

    g_vals[1] = 1;  /* the section accepts writes again */
    CHECK(g_vals[1] == 1, "section unlocked after the verification");

    printf(failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
    return failures != 0;
}
