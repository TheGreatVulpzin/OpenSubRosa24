/* Test of the scenario forcer (instrumentation/hook/force.c) on a small fake game image.
 *
 * The test writes subvulpes_force.txt next to its own executable, moves a fake tick counter
 * and a fake state, and checks what the forcer writes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hook.h"

uintptr_t sr_base;

void sv_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void sv_console_event(int error, const char *fmt, ...)
{
    (void)error;
    (void)fmt;
}

#define RVA_TICKS 0x10u
#define RVA_STATE 0x20u
#define RVA_TIMER 0x24u
#define RVA_SMALL 0x30u

static int failures;

static void check(int ok, const char *label)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        failures++;
}

static uint8_t g_image[256];

static uint32_t get(uint32_t rva)
{
    uint32_t v = 0;
    memcpy(&v, g_image + rva, 4);
    return v;
}

static void put(uint32_t rva, uint32_t v)
{
    memcpy(g_image + rva, &v, 4);
}

static void tick_to(uint32_t ticks)
{
    put(RVA_TICKS, ticks);
    sv_force_check();
}

int main(void)
{
    char path[MAX_PATH];
    FILE *f;
    char *slash;

    sr_base = (uintptr_t)g_image;
    GetModuleFileNameA(NULL, path, sizeof path);
    slash = strrchr(path, '\\');
    strcpy(slash + 1, "subvulpes_force.txt");
    f = fopen(path, "w");
    if (!f) {
        printf("FAIL: cannot write %s\n", path);
        return 1;
    }
    fprintf(f, "# test\nticks 0x10\n");
    fprintf(f, "at 100 set 0x30 1 7   # one byte\n");
    fprintf(f, "every 600 if 0x20 4 2 set 0x24 4 300\n");
    fprintf(f, "every 600 if 0x20 4 3 set 0x24 4 60\n");
    fprintf(f, "at 5 set 0x50 3 1\n");        /* bad size: ignored */
    fprintf(f, "nonsense line here\n");       /* bad rule: ignored */
    fclose(f);

    put(RVA_STATE, 1);
    put(RVA_TIMER, 3600);
    tick_to(1);
    check(get(RVA_TIMER) == 3600, "nothing fires before its tick or condition");
    tick_to(100);
    check((get(RVA_SMALL) & 0xff) == 7 && (get(RVA_SMALL) >> 8) == 0, "an at rule writes one byte at its tick");
    put(RVA_SMALL, 0);
    tick_to(101);
    check(get(RVA_SMALL) == 0, "an at rule fires one time");

    put(RVA_STATE, 2);
    put(RVA_TIMER, 9000);
    tick_to(200);
    check(get(RVA_TIMER) == 300, "an every rule fires when its condition holds");
    put(RVA_TIMER, 250);
    tick_to(300);
    check(get(RVA_TIMER) == 250, "an every rule waits for its period");
    tick_to(850);
    check(get(RVA_TIMER) == 300, "an every rule fires again after its period");

    put(RVA_STATE, 3);
    put(RVA_TIMER, 1260);
    tick_to(900);
    check(get(RVA_TIMER) == 60, "an every rule fires at the first tick its condition holds");
    put(RVA_TIMER, 1260);
    tick_to(950);
    check(get(RVA_TIMER) == 1260, "the rule of state 3 waits for its period");
    tick_to(1500);
    check(get(RVA_TIMER) == 60, "the rule of state 3 fires again after its period");

    /* The game sets the counter back at each new round. The rule of state 2 would wait until tick 2100
     * if the forcer did not notice this. */
    put(RVA_STATE, 1);
    tick_to(0);
    put(RVA_STATE, 2);
    put(RVA_TIMER, 9000);
    tick_to(1);
    check(get(RVA_TIMER) == 300, "after the counter goes back an every rule fires again");

    remove(path);
    printf(failures ? "FAILED: %d\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
