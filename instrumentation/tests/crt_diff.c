/* crt_diff: compares the CRT functions of the game (MSVCR100.dll) with the new CRT
 * (ucrtbase.dll) on many inputs.
 *
 * The x87 math intrinsics come from ucrtbase.dll. The other functions come from the CRT that
 * this test links statically, the same CRT as the server (sscanf and sprintf of the new CRT
 * are inline functions, not DLL exports).
 *
 * Phase 5.2 (docs/plano.md) links the server to the new CRT. A function can move only when
 * it gives the same bits as MSVCR100 for the inputs of the game: the x87 math intrinsics
 * (physics), sscanf (the numbers of config.txt and the data files), sprintf, rand, qsort.
 *
 * Usage: crt_diff.exe [count]   (default 2000000 inputs for each math function)
 * The x87 precision is 53 bits, as in the game.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning(disable : 4100 4152) /* naked thunks, function pointers from GetProcAddress */

static HMODULE g_old, g_new;  /* MSVCR100.dll, ucrtbase.dll */

static void *fn(HMODULE m, const char *name)
{
    void *p = (void *)GetProcAddress(m, name);
    if (!p) {
        printf("missing: %s in %s\n", name, m == g_old ? "MSVCR100.dll" : "ucrtbase.dll");
        exit(1);
    }
    return p;
}

/* ------------------------------------------------------------ inputs */

static uint64_t g_rng = 0x9e3779b97f4a7c15ull;

static uint64_t next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return g_rng;
}

/* A double in [lo, hi). Half of the inputs are floats widened to double, as most game
 * values are floats. */
static double in_range(double lo, double hi)
{
    double d = lo + (hi - lo) * ((next() >> 11) * (1.0 / 9007199254740992.0));
    return (next() & 1) ? (double)(float)d : d;
}

/* ------------------------------------------------------------ x87 intrinsics */

static void *g_call;

static __declspec(naked) double __cdecl call_one(double x)
{
    __asm fld qword ptr [esp + 4]
    __asm jmp dword ptr [g_call]
}

static __declspec(naked) double __cdecl call_two(double a, double b)
{
    __asm fld qword ptr [esp + 4]
    __asm fld qword ptr [esp + 12]
    __asm jmp dword ptr [g_call]
}

static int same_bits(double a, double b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static void math_one(const char *name, double lo, double hi, int count)
{
    void *a = fn(g_old, name), *b = fn(g_new, name);
    int i, diff = 0;
    for (i = 0; i < count; i++) {
        double x = in_range(lo, hi), ra, rb;
        g_call = a;
        ra = call_one(x);
        g_call = b;
        rb = call_one(x);
        if (!same_bits(ra, rb) && !(ra != ra && rb != rb)) {
            if (diff < 3)
                printf("    %s(%.17g): %.17g and %.17g\n", name, x, ra, rb);
            diff++;
        }
    }
    printf("%-9s [%g, %g): %d of %d differ\n", name, lo, hi, diff, count);
}

static void math_two(const char *name, double lo1, double hi1, double lo2, double hi2, int count)
{
    void *a = fn(g_old, name), *b = fn(g_new, name);
    int i, diff = 0;
    for (i = 0; i < count; i++) {
        double x = in_range(lo1, hi1), y = in_range(lo2, hi2), ra, rb;
        g_call = a;
        ra = call_two(x, y);
        g_call = b;
        rb = call_two(x, y);
        if (!same_bits(ra, rb) && !(ra != ra && rb != rb)) {
            if (diff < 3)
                printf("    %s(%.17g, %.17g): %.17g and %.17g\n", name, x, y, ra, rb);
            diff++;
        }
    }
    printf("%-9s [%g, %g) x [%g, %g): %d of %d differ\n", name, lo1, hi1, lo2, hi2, diff, count);
}

/* ------------------------------------------------------------ text and other functions */

typedef int(__cdecl *sscanf_t)(const char *, const char *, ...);
typedef int(__cdecl *sprintf_t)(char *, const char *, ...);

static void scan_floats(int count)
{
    sscanf_t a = (sscanf_t)fn(g_old, "sscanf"), b = sscanf;
    static const char *formats[] = { "%.9g", "%.6f", "%.3f", "%.2f", "%g", "%.17g", "%f" };
    int i, diff = 0;
    for (i = 0; i < count; i++) {
        char text[64];
        float fa, fb;
        double v = in_range(-100000, 100000) / (double)(1 << (next() % 16));
        snprintf(text, sizeof text, formats[next() % (sizeof formats / sizeof *formats)], v);
        fa = fb = 0;
        a(text, "%f", &fa);
        b(text, "%f", &fb);
        if (memcmp(&fa, &fb, sizeof fa)) {
            if (diff < 3)
                printf("    sscanf(\"%s\", %%f): %.9g and %.9g\n", text, fa, fb);
            diff++;
        }
    }
    printf("sscanf %%f: %d of %d differ\n", diff, count);
}

static void print_formats(int count)
{
    sprintf_t a = (sprintf_t)fn(g_old, "sprintf"), b = sprintf;
    static const char *formats[] = { "%d", "%f", "%.1f", "%.2f", "%g", "%e", "%.0f", "%5.1f" };
    int i, diff = 0;
    for (i = 0; i < count; i++) {
        char ta[128], tb[128];
        const char *f = formats[next() % (sizeof formats / sizeof *formats)];
        double v = in_range(-1e7, 1e7) / (double)(1 << (next() % 20));
        if (f[1] == 'd') {
            a(ta, f, (int)v);
            b(tb, f, (int)v);
        } else {
            a(ta, f, v);
            b(tb, f, v);
        }
        if (strcmp(ta, tb)) {
            if (diff < 3)
                printf("    sprintf(\"%s\"): \"%s\" and \"%s\"\n", f, ta, tb);
            diff++;
        }
    }
    printf("sprintf: %d of %d differ\n", diff, count);
}

static void random_numbers(void)
{
    void(__cdecl * sa)(unsigned) = (void(__cdecl *)(unsigned))fn(g_old, "srand");
    void(__cdecl * sb)(unsigned) = srand;
    int(__cdecl * ra)(void) = (int(__cdecl *)(void))fn(g_old, "rand");
    int(__cdecl * rb)(void) = rand;
    unsigned seed;
    int i, diff = 0;
    for (seed = 0; seed < 200; seed++) {
        sa(seed * 7919u);
        sb(seed * 7919u);
        for (i = 0; i < 1000; i++)
            diff += ra() != rb();
    }
    printf("rand: %d of 200000 differ\n", diff);
}

/* The game sorts names with an unsigned byte compare (RVA 0xd320). Many equal keys show
 * whether both qsort versions give the same order for equal elements. */
typedef struct { unsigned char key; int id; } Item;

static int __cdecl by_key(const void *a, const void *b)
{
    int x = ((const Item *)a)->key, y = ((const Item *)b)->key;
    return x < y ? -1 : x > y;
}

static void sort_order(void)
{
    void(__cdecl * qa)(void *, size_t, size_t, int(__cdecl *)(const void *, const void *)) = fn(g_old, "qsort");
    void(__cdecl * qb)(void *, size_t, size_t, int(__cdecl *)(const void *, const void *)) = qsort;
    static Item a[300], b[300];
    int run, i, diff = 0;
    for (run = 0; run < 2000; run++) {
        int n = 1 + (int)(next() % 300);
        for (i = 0; i < n; i++) {
            a[i].key = (unsigned char)(next() % 8);
            a[i].id = i;
        }
        memcpy(b, a, sizeof a);
        qa(a, n, sizeof *a, by_key);
        qb(b, n, sizeof *b, by_key);
        for (i = 0; i < n; i++)
            if (a[i].id != b[i].id) {
                diff++;
                break;
            }
    }
    printf("qsort with equal keys: %d of 2000 arrays in a different order\n", diff);
}

static void small_functions(void)
{
    int(__cdecl * la)(int) = (int(__cdecl *)(int))fn(g_old, "tolower");
    int(__cdecl * lb)(int) = tolower;
    double(__cdecl * fa)(double) = (double(__cdecl *)(double))fn(g_old, "floor");
    double(__cdecl * fb)(double) = floor;
    int c, i, diff = 0;
    for (c = -1; c < 256; c++)
        diff += la(c) != lb(c);
    printf("tolower: %d of 257 differ\n", diff);
    diff = 0;
    for (i = 0; i < 1000000; i++) {
        double x = in_range(-1e6, 1e6);
        double ra = fa(x), rb = fb(x);
        diff += !same_bits(ra, rb);
    }
    printf("floor: %d of 1000000 differ\n", diff);
}

int main(int argc, char **argv)
{
    unsigned cw;
    int n = argc > 1 ? atoi(argv[1]) : 2000000;
    g_old = LoadLibraryA("MSVCR100.dll");
    g_new = LoadLibraryA("ucrtbase.dll");
    if (!g_old || !g_new) {
        printf("cannot load MSVCR100.dll or ucrtbase.dll\n");
        return 1;
    }
    _controlfp_s(&cw, _PC_53, _MCW_PC);

    math_one("_CIsqrt", 0, 1e6, n);
    math_one("_CIsin", -7, 7, n);
    math_one("_CIsin", -1000, 1000, n);
    math_one("_CIcos", -7, 7, n);
    math_one("_CIcos", -1000, 1000, n);
    math_one("_CItan", -1.6, 1.6, n);
    math_one("_CIacos", -1, 1, n);
    math_two("_CIatan2", -100, 100, -100, 100, n);
    math_two("_CIpow", 0, 100, -4, 4, n);
    math_two("_CIpow", -10, 10, 0, 3, n / 4);

    scan_floats(n / 4);
    print_formats(n / 4);
    random_numbers();
    sort_order();
    small_functions();
    return 0;
}
