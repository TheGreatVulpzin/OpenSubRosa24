/* The main function and the main loop of the dedicated server.
 *
 * sr_getpaths sets up the folders, the timers and the settings, then calls
 * sr_dedicatedserverloop. The loop runs one tick every 16 ms. In the hook
 * build, own_main=1 in subvulpes.ini replaces the two game functions with these.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SR_STANDALONE
#include "hook.h"
#endif
#include "block_mesh.h"
#include "bonds.h"
#include "bullets.h"
#include "events.h"
#include "humans.h"
#include "items.h"
#include "level.h"
#include "logic.h"
#include "objects.h"
#include "options.h"
#include "particles.h"
#include "player.h"
#include "random.h"
#include "system.h"
#include "profiler.h"
#include "server.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "sr_server.h"

#ifndef SR_STANDALONE
#include "loop_calls.h"
#endif

void *sr_import(const char *dll, const char *name);
void sv_hooks_lifted_profile(void);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

#define G(type, rva) (*(type *)(sr_base + (rva)))
#ifdef SR_STANDALONE
int sv_stop_requested(void);
#endif

/* ------------------------------------------------------------ globals (RVA) */

#define g_sysinfo (*SR_AT(SysInfo, SR_RVA_SYSINFO))
#define g_debug (*SR_AT(Debug, SR_RVA_DEBUG))
#define g_debugit G(int, 0x204020)
#define g_dedicated G(int, 0x204380) /* 1 once the server is set up */
#define g_numofstaticbonds G(int, SR_RVA_NUMOFSTATICBONDS)

/* The pointers that the game uses to read and write its files. */
#define game_fread_ptr G(void *, 0x91022a4)
#define game_fwrite_ptr G(void *, 0x13bf2d00)
/* The WSADATA record that WSAStartup fills. */
#define wsadata ((void *)(sr_base + 0xac218e0))

/* ------------------------------------------------------------ CRT and WinSock of the game */

/* The game CRT (MSVCR100.dll): its stdout, its current folder and its seek. */
static int(__cdecl *g_printf)(const char *, ...);
static int(__cdecl *g_fseek)(void *, long, int);
static char *(__cdecl *g_getcwd)(char *, int);
static int(WINAPI *g_wsastartup)(WORD, void *);

static void load_imports(void)
{
#define IMP(var, name) *(void **)&var = sr_import("MSVCR100.dll", name)
    IMP(g_printf, "printf");
    IMP(g_fseek, "fseek");
    IMP(g_getcwd, "_getcwd");
#undef IMP
    *(void **)&g_wsastartup = sr_import_ordinal("WSOCK32.dll", 115);
}

/* ------------------------------------------------------------ main loop */

static int simulating(int state)
{
    return state == 0 || state == 1 || state == 2;
}

/* One server tick (16 ms). */
static void tick(void)
{
    int i, state;

    if (sr_game.state == 2) {
        for (i = 0; i < 256; i++) {
            if (sr_players[i].active != 0 && sr_players[i].is_bot != 0)
                playerai(i);
        }
    }
    servercycle();
    serverrecv();

    nametimer(g_timerlist.physicssim, "physicssim");
    starttimer(g_timerlist.physicssim);
    playersimulation();
    if (simulating(sr_game.state)) {
        g_numofstaticbonds = sr_numofbonds;
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
    stoptimer(g_timerlist.physicssim);

    /* Every 64 ticks the physics time goes to the log text, and to the log file when the
     * debug flag is on. */
    if ((sr_game.ticks & 0x3f) == 0) {
        sprintf(g_debug.log_text, "physics:%d", g_timer[g_timerlist.physicssim].ms);
        if (g_debugit != 0) {
            g_debug.log_file = os_fopen(g_debug.log_path, "rb+");
            if (g_debug.log_file) {
                g_fseek(g_debug.log_file, 0, 2);
                os_fprintf(g_debug.log_file, "%s\r\n", g_debug.log_text);
                os_fclose(g_debug.log_file);
            }
        }
    }
    serversend();
    sr_game.ticks++;
    if (simulating(sr_game.state))
        bulletsimulation();
    /* The state is read before eventsimulation, which can change it. */
    state = sr_game.state;
    eventsimulation();
    if (simulating(state))
        bullettimetolive();
}

/* Sets the server up from the settings, then runs the ticks. */
void __cdecl sr_dedicatedserverloop(void)
{
    uint32_t next, now, status_ms;
    int i, hours, catchup, status_ticks;

    g_printf(getmasterserverip() ? "connection successful\n" : "connection failed\n");

    if (sr_option.roundtime < 1)
        sr_option.roundtime = 1;
    else if (sr_option.roundtime > 30)
        sr_option.roundtime = 30;
    hours = sr_option.roundtime;
    if (sr_option.maxnumofplayers < 2)
        sr_option.maxnumofplayers = 2;
    else if (sr_option.maxnumofplayers > 128)
        sr_option.maxnumofplayers = 128;
    sr_serverinfo.max_players = sr_option.maxnumofplayers;
    strcpy(sr_serverinfo.name, sr_option.servername);
    sr_game.round_time = hours * 3600;
    sr_game.wait_time = 3600;
    sr_game.end_time = 1260;
    sr_option.net_on = 1;
    sr_option.master_on = 1;
    disablesocket(0);
    sr_serverinfo.active = 1;
    sr_serverinfo.unused_08 = 1;
    g_printf("port:%d\n", (unsigned)(uint16_t)sr_bsdsocket.port);
    g_printf("socketenabled:%d\n", sr_bsdsocket.enabled[0]);
    init_genrand((int)os_time64(NULL));
    sr_loadaccountsserver();
    leveleditor_load();
    setupgame();
    for (i = 0; i < 4; i++)
        sr_market.stocks[i].price = 100.0f;
    for (i = 0; i < 256; i++)
        sr_players[i].active = 0;
    sr_game.gametype = sr_option.gametype;
    if (sr_option.gametype != 3 && sr_option.gametype != 4)
        sr_game.gametype = 3;
    resetgame();
    g_dedicated = 1;

    /* 16 ms ticks. Behind by 16 ms or more: run up to 5 ticks, then wait again. After the
     * third tick in a row, the schedule restarts from now instead of catching up. */
    next = systemtimer();
    status_ms = next;
    status_ticks = sr_game.ticks;
    sv_log("Server started on port %u.", (unsigned)(uint16_t)sr_bsdsocket.port);
    for (;;) {
#ifdef SR_STANDALONE
        if (sv_stop_requested())
            return;
#endif
        os_sleep(1);
        now = systemtimer();
        /* A status line every minute, so a log shows that this loop runs and how fast. */
        if (now - status_ms >= 60000) {
            uint32_t ms = now - status_ms;
            int p, players = 0, bots = 0;
            for (p = 0; p < 256; p++) {
                if (sr_players[p].active)
                    sr_players[p].is_bot ? bots++ : players++;
            }
            sv_log("Server status: %.1f ticks/s, %d players, %d bots",
                   (sr_game.ticks - status_ticks) * 1000.0 / ms, players, bots);
            sv_hooks_lifted_profile();  /* profile=1 in subvulpes.ini */
            status_ms += ms;
            status_ticks = sr_game.ticks;
        }
        catchup = 0;
        for (;;) {
            if (now - next < 16 || catchup > 4)
                break;
            catchup++;
            if (catchup < 4)
                next += 16;
            else
                next = systemtimer();
            tick();
            now = systemtimer();
        }
    }
}

/* The main function: folders, timers and settings, then the loop. */
void __cdecl sr_getpaths(void)
{
    load_imports();
    debug_resetlogfile();
    getcpufrequency();
    setuptimers();
    G(int, 0xb7a8c84) = 0; /* a game counter that starts at 0 */
    /* The game reads files through these two pointers (loadblock and others). */
    game_fread_ptr = sr_import("MSVCR100.dll", "fread");
    game_fwrite_ptr = sr_import("MSVCR100.dll", "fwrite");
    g_getcwd(g_sysinfo.start_dir, 0x200);
    g_getcwd(g_sysinfo.config_dir, 0x200);
    loadsaveoptions(0);
    g_wsastartup(0x101, wsadata);
    sr_dedicatedserverloop();
}

#ifndef SR_STANDALONE
/* ------------------------------------------------------------ hooks */

static uint8_t expect_loop[6] = { 0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8 };
static uint8_t expect_main[5] = { 0xe8, 0x6b, 0x85, 0xfa, 0xff };
static const uint8_t mask_all[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static SvHook g_main_hooks[] = {
    { "own_getpaths", 0x5cd10, (void *)(uintptr_t)sr_getpaths, 5, expect_main, mask_all, NULL, 0, 0 },
    { "own_dedicatedserverloop", 0x5c8d0, (void *)(uintptr_t)sr_dedicatedserverloop, 6, expect_loop, mask_all,
      NULL, 0, 0 },
};

/* subvulpes.ini own_main=1: the server runs these two functions instead of the game code. */
int sv_main_loop_install(void)
{
    if (!sv_config_int("own_main", 0))
        return 0;
    load_imports();
    return sv_install_hooks(g_main_hooks, 2);
}
#endif /* SR_STANDALONE */
