/* The main function and the main loop of the dedicated server, written by hand.
 *
 * Originals in the 024d: getpaths (RVA 0x5cd10, called by the CRT entry point) and
 * dedicatedserverloop (RVA 0x5c8d0). Both never return. The lifter does not convert them:
 * they are the whole server, so a record of one call would be a whole session.
 *
 * The code does the same steps in the same order as the original. Game functions are called
 * through their address in the 024d, so the hooks of the lifted functions still apply.
 * subvulpes.ini own_main=1 installs these two as hooks (default 0: the original runs).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <intrin.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "sr_runtime.h"
#include "sr_server.h"

void *sr_import(const char *dll, const char *name);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

#define G(type, rva) (*(type *)(sr_base + (rva)))
#define CALL(rva) ((void(__cdecl *)(void))(sr_base + (rva)))

/* ------------------------------------------------------------ globals (RVA) */

/* config.txt values (loadsaveoptions) */
#define cfg_servername      ((char *)(sr_base + 0x90dc388))
#define cfg_408             G(int, 0x90dc408)
#define cfg_40c             G(int, 0x90dc40c)
#define cfg_maxplayers      G(int, 0x90dc454)   /* maxnumofplayers */
#define cfg_gametype        G(int, 0x90dc458)
#define cfg_roundtime       G(int, 0x90dc45c)   /* roundtime, in hours */

/* game (RVA 0x90dc540, struct Game in sr_types.h) */
#define game_28             G(int, 0x90dc568)
#define game_34             G(int, 0x90dc574)

/* server (RVA 0x761ea60) */
#define server_active       G(int, 0x761ea60)
#define server_08           G(int, 0x761ea68)
#define server_name         ((char *)(sr_base + 0x761ea74))
#define server_maxplayers   G(int, 0x761eabc)

/* bsdsocket (RVA 0x1952d680) */
#define socket_enabled      G(int, 0x1952d684)
#define socket_port         G(uint16_t, 0x1952d6d8)

#define qpc_to_ms           G(double, 0x916d6f0)  /* milliseconds per QueryPerformanceCounter tick */
#define flag_204380         G(int, 0x204380)
#define bond_count_copy     G(int, 0x13c22d24)
#define numofbonds          G(int, 0x17179a28)
#define unk_17179604(i)     G(float, 0x17179604 + (i) * 0x14)

/* Profiler timers (RVA 0x14043a0, 0x58 bytes each): name[0x40], start (RDTSC) at +0x40. */
#define TIMER(i)            ((uint8_t *)(sr_base + 0x14043a0 + (i) * 0x58))
#define timer_current       G(int, 0x12a81c9c)

/* Physics time log, every 64 ticks when debug logging is on. */
#define debug_log           G(int, 0x204020)
#define log_path            ((char *)(sr_base + 0x15cbf24))
#define log_text            ((char *)(sr_base + 0x15cbe24))
#define log_file            G(void *, 0x15cbf64)

/* ------------------------------------------------------------ game functions (RVA) */

#define getmasterserverip   ((int(__cdecl *)(void))(sr_base + 0x1ac40))
#define disablesocket       ((void(__cdecl *)(int))(sr_base + 0xd710))
#define init_genrand        ((void(__cdecl *)(int))(sr_base + 0xad30))
#define loadaccountsserver  CALL(0xe2e0)
#define leveleditor_load    CALL(0x8590)
#define setupgame           CALL(0x502a0)
#define resetgame           CALL(0x590f0)
#define playerai            ((void(__cdecl *)(int))(sr_base + 0x4f720))
#define servercycle         CALL(0x33570)
#define serverrecv          CALL(0x33650)
#define playersimulation    CALL(0xa2a0)
#define particlesimulation  CALL(0x27ba0)
#define objectsimulation    CALL(0x56710)
#define humansimulation     CALL(0x51060)
#define itemsimulation      CALL(0x542a0)
#define bondsimulation2     CALL(0x41de0)
#define itemlogic           CALL(0x4c8c0)
#define itemtimetolive      CALL(0x30ab0)
#define objecttimetolive    CALL(0x32340)
#define particletimetolive  CALL(0x279b0)
#define bondtimetolive      CALL(0x12c90)
#define logicsimulation     CALL(0x5c580)
#define stoptimer           ((void(__cdecl *)(int))(sr_base + 0xb3f0))
#define serversend          CALL(0x4ed30)
#define bulletsimulation    CALL(0x48ec0)
#define eventsimulation     CALL(0x61a0)
#define bullettimetolive    CALL(0x14590)
#define debug_resetlogfile  CALL(0x5280)
#define getcpufrequency     CALL(0xb4a0)
#define setuptimers         CALL(0xb540)
#define loadsaveoptions     ((void(__cdecl *)(int))(sr_base + 0x4e7a0))

/* ------------------------------------------------------------ CRT and WinSock of the game */

/* The game CRT (MSVCR100.dll): its stdout, its FILE objects and its heap. */
static int(__cdecl *g_printf)(const char *, ...);
static int(__cdecl *g_sprintf)(char *, const char *, ...);
static void *(__cdecl *g_fopen)(const char *, const char *);
static int(__cdecl *g_fseek)(void *, long, int);
static int(__cdecl *g_fprintf)(void *, const char *, ...);
static int(__cdecl *g_fclose)(void *);
static __int64(__cdecl *g_time64)(__int64 *);
static void(__cdecl *g_sleep)(unsigned long);
static char *(__cdecl *g_getcwd)(char *, int);
static int(WINAPI *g_wsastartup)(WORD, void *);

static void load_imports(void)
{
#define IMP(var, name) *(void **)&var = sr_import("MSVCR100.dll", name)
    IMP(g_printf, "printf");
    IMP(g_sprintf, "sprintf");
    IMP(g_fopen, "fopen");
    IMP(g_fseek, "fseek");
    IMP(g_fprintf, "fprintf");
    IMP(g_fclose, "fclose");
    IMP(g_time64, "_time64");
    IMP(g_sleep, "_sleep");
    IMP(g_getcwd, "_getcwd");
#undef IMP
    *(void **)&g_wsastartup = sr_import_ordinal("WSOCK32.dll", 115);
}

/* ------------------------------------------------------------ main loop */

/* The milliseconds of the performance counter, as the original: fild, fmul, fistp
 * (round to nearest with the x87 control word of the game). 0 when the call fails. */
static uint32_t now_ms(void)
{
    LARGE_INTEGER c;
    double factor = qpc_to_ms;
    __int64 r;
    if (!QueryPerformanceCounter(&c))
        return 0;
    __asm {
        fild qword ptr [c]
        fmul qword ptr [factor]
        fistp qword ptr [r]
    }
    return (uint32_t)r;
}

static int simulating(int state)
{
    return state == 0 || state == 1 || state == 2;
}

/* One server tick (16 ms). */
static void tick(void)
{
    int i, state;
    uint8_t *t;
    uint64_t tsc;

    if (sr_game.state == 2) {
        for (i = 0; i < 256; i++) {
            const uint8_t *p = (const uint8_t *)&sr_players[i];
            if (*(const int *)p != 0 && *(const int *)(p + 0x1a38) != 0)
                playerai(i);
        }
    }
    servercycle();
    serverrecv();

    /* Profiler: start the "physicssim" timer. */
    t = TIMER(timer_current);
    memcpy(t, "physicssim", sizeof "physicssim");
    *(uint32_t *)(t + 0x48) = 0;
    *(uint32_t *)(t + 0x4c) = 0;
    *(uint32_t *)(t + 0x50) = 0;
    tsc = __rdtsc();
    *(uint32_t *)(t + 0x40) = (uint32_t)tsc;
    *(uint32_t *)(t + 0x44) = (uint32_t)(tsc >> 32);

    playersimulation();
    if (simulating(sr_game.state)) {
        bond_count_copy = numofbonds;
        particlesimulation();
        objectsimulation();
        humansimulation();
        itemsimulation();
        bondsimulation2();
        itemlogic();
        itemtimetolive();
        objecttimetolive();
        particletimetolive();
        bondtimetolive();
    }
    logicsimulation();
    stoptimer(timer_current);

    if ((sr_game.ticks & 0x3f) == 0) {
        g_sprintf(log_text, "physics:%d", *(int *)(TIMER(timer_current) + 0x50));
        if (debug_log != 0) {
            log_file = g_fopen(log_path, "rb+");
            if (log_file) {
                g_fseek(log_file, 0, 2);
                g_fprintf(log_file, "%s\r\n", log_text);
                g_fclose(log_file);
            }
        }
    }
    serversend();
    sr_game.ticks++;
    if (simulating(sr_game.state))
        bulletsimulation();
    state = sr_game.state;  /* read before eventsimulation, as the original */
    eventsimulation();
    if (simulating(state))
        bullettimetolive();
}

/* Original: dedicatedserverloop (RVA 0x5c8d0). */
void __cdecl sr_dedicatedserverloop(void)
{
    uint32_t next, status_ms;
    int i, hours, catchup, status_ticks;

    g_printf(getmasterserverip() ? "connection successful\n" : "connection failed\n");

    if (cfg_roundtime < 1)
        cfg_roundtime = 1;
    else if (cfg_roundtime > 30)
        cfg_roundtime = 30;
    hours = cfg_roundtime;
    if (cfg_maxplayers < 2)
        cfg_maxplayers = 2;
    else if (cfg_maxplayers > 128)
        cfg_maxplayers = 128;
    server_maxplayers = cfg_maxplayers;
    strcpy(server_name, cfg_servername);
    sr_game.round_time = hours * 3600;
    game_28 = 3600;
    game_34 = 1260;
    cfg_408 = 1;
    cfg_40c = 1;
    disablesocket(0);
    server_active = 1;
    server_08 = 1;
    g_printf("port:%d\n", (unsigned)socket_port);
    g_printf("socketenabled:%d\n", socket_enabled);
    init_genrand((int)g_time64(NULL));
    loadaccountsserver();
    leveleditor_load();
    setupgame();
    for (i = 0; i < 4; i++)
        unk_17179604(i) = 100.0f;
    for (i = 0; i < 256; i++)
        *(int *)&sr_players[i] = 0;
    sr_game.gametype = cfg_gametype;
    if (cfg_gametype != 3 && cfg_gametype != 4)
        sr_game.gametype = 3;
    resetgame();
    flag_204380 = 1;

    /* 16 ms ticks. Behind by 16 ms or more: run up to 5 ticks, then wait again. After the
     * fourth tick in a row, the schedule restarts from now instead of catching up. */
    next = now_ms();
    status_ms = next;
    status_ticks = sr_game.ticks;
    sv_log("own main loop: running (game ticks %d)", sr_game.ticks);
    for (;;) {
        g_sleep(1);
        /* A status line every minute, so a log shows that this loop runs and how fast. */
        if (now_ms() - status_ms >= 60000) {
            uint32_t ms = now_ms() - status_ms;
            sv_log("own main loop: %d ticks in %u ms (%.1f ticks/s)", sr_game.ticks - status_ticks, ms,
                   (sr_game.ticks - status_ticks) * 1000.0 / ms);
            status_ms += ms;
            status_ticks = sr_game.ticks;
        }
        catchup = 0;
        for (;;) {
            if (now_ms() - next < 16 || catchup > 4)
                break;
            catchup++;
            if (catchup < 4)
                next += 16;
            else
                next = now_ms();
            tick();
        }
    }
}

/* Original: getpaths (RVA 0x5cd10), the main function. */
void __cdecl sr_getpaths(void)
{
    sv_log("own main: getpaths runs");
    load_imports();
    debug_resetlogfile();
    getcpufrequency();
    setuptimers();
    G(int, 0xb7a8c84) = 0;
    /* The game reads files through these two pointers (loadblock and others). */
    G(void *, 0x91022a4) = sr_import("MSVCR100.dll", "fread");
    G(void *, 0x13bf2d00) = sr_import("MSVCR100.dll", "fwrite");
    g_getcwd((char *)(sr_base + 0x916d2e0), 0x200);
    g_getcwd((char *)(sr_base + 0x916d4e0), 0x200);
    loadsaveoptions(0);
    g_wsastartup(0x101, (void *)(sr_base + 0xac218e0));
    sr_dedicatedserverloop();
}

/* ------------------------------------------------------------ hooks */

static uint8_t expect_loop[6] = { 0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8 };
static uint8_t expect_main[5] = { 0xe8, 0x6b, 0x85, 0xfa, 0xff };
static const uint8_t mask_all[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static SvHook g_main_hooks[] = {
    { "own_getpaths", 0x5cd10, (void *)(uintptr_t)sr_getpaths, 5, expect_main, mask_all, NULL, 0, 0 },
    { "own_dedicatedserverloop", 0x5c8d0, (void *)(uintptr_t)sr_dedicatedserverloop, 6, expect_loop, mask_all,
      NULL, 0, 0 },
};

/* subvulpes.ini own_main=1: the server runs these two functions instead of the originals. */
int sv_main_loop_install(void)
{
    if (!sv_config_int("own_main", 0))
        return 0;
    load_imports();
    return sv_install_hooks(g_main_hooks, 2);
}
