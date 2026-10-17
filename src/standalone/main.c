/* subvulpes-server: the dedicated server without subrosadedicated024d.exe.
 *
 * Phase 5.1 (docs/plano.md). The game code is the lifted C and the hand-written C. Its
 * memory is a region with the layout of the 024d image: SR_VA() and the g_ globals work as
 * with the hooks. The game imports (MSVCR100, WSOCK32, KERNEL32) come from
 * src/platform/plat_imports.c (Phase 5.2): the server does not need MSVCR100.dll.
 * Run it in the folder of a server (config.txt, data/, user/).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <dbghelp.h>

#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dbghelp.lib")

#include "image_024d.h"
#include "platform.h"

uintptr_t sr_base;  /* the image region (map_image) */
void sr_fn_init(void);
void lf_x87_init(void);
void __cdecl sr_getpaths(void);

/* ------------------------------------------------------------ console and log */

/* SV_LOG=<file>: a copy of the console output. The console stays visible, as with the
 * original server. Each line goes to the disk at once, so a crash loses no line. */
static FILE *g_log;

static void sv_out(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
    if (g_log) {
        fputs(s, g_log);
        fflush(g_log);
    }
}

void sv_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    memcpy(line, "[subvulpes] ", 12);
    vsnprintf(line + 12, sizeof line - 13, fmt, ap);
    va_end(ap);
    strcat(line, "\n");
    sv_out(line);
}

/* The printf of the game (port, socketenabled, master server). It replaces the printf of
 * MSVCR100, which writes to its own stdout and cannot copy the text to the log. */
static int __cdecl sv_printf(const char *fmt, ...)
{
    char text[1024];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    sv_out(text);
    return n;
}

static void open_log(void)
{
    const char *path = getenv("SV_LOG");
    if (path && !(g_log = fopen(path, "w")))
        sv_log("cannot open the log file %s", path);
}

/* ------------------------------------------------------------ packet capture */

/* SV_PACKETS=<file>: each UDP datagram that the server receives or sends, in the format of
 * hook/capture.c (tools/net/capture.py reads it). The file has player names and chat. */
#define RVA_GAME_TICKS (0x90dc540u + 0x38)

static FILE *g_packets;
static DWORD g_packets_start, g_packets_flush;
static int(WINAPI *g_recvfrom)(UINT_PTR, char *, int, int, void *, int *);
static int(WINAPI *g_sendto)(UINT_PTR, const char *, int, int, const void *, int);

static void capture(uint8_t dir, const void *data, int len, const void *addr)
{
    const uint8_t *sa = (const uint8_t *)addr;
    uint32_t t = GetTickCount() - g_packets_start;
    uint32_t tick = *(const uint32_t *)(sr_base + RVA_GAME_TICKS), ip = 0;
    uint16_t port = 0, n = (uint16_t)len;
    if (len <= 0 || len > 0xffff)
        return;
    if (sa && sa[0] == 2 && sa[1] == 0) {  /* sockaddr_in: AF_INET, port (big endian), address */
        port = (uint16_t)(sa[2] << 8 | sa[3]);
        memcpy(&ip, sa + 4, 4);
    }
    fwrite(&dir, 1, 1, g_packets);
    fwrite(&t, 4, 1, g_packets);
    fwrite(&tick, 4, 1, g_packets);
    fwrite(&ip, 4, 1, g_packets);
    fwrite(&port, 2, 1, g_packets);
    fwrite(&n, 2, 1, g_packets);
    fwrite(data, 1, n, g_packets);
    /* The launcher stops the server with TerminateProcess: flush each second, so that a stop
     * loses at most one second of packets. */
    if (t - g_packets_flush >= 1000) {
        fflush(g_packets);
        g_packets_flush = t;
    }
}

static int WINAPI capture_recvfrom(UINT_PTR s, char *buf, int len, int flags, void *from, int *fromlen)
{
    int n = g_recvfrom(s, buf, len, flags, from, fromlen);
    if (n > 0)
        capture(0, buf, n, from);
    return n;
}

static int WINAPI capture_sendto(UINT_PTR s, const char *buf, int len, int flags, const void *to, int tolen)
{
    int n = g_sendto(s, buf, len, flags, to, tolen);
    if (n > 0)
        capture(1, buf, n, to);
    return n;
}

static void open_packets(void)
{
    const char *path = getenv("SV_PACKETS");
    uint32_t version = 1, base = (uint32_t)sr_base;
    if (!path)
        return;
    if (!(g_packets = fopen(path, "wb"))) {
        sv_log("cannot open the packet file %s", path);
        return;
    }
    fwrite("SVPK", 4, 1, g_packets);
    fwrite(&version, 4, 1, g_packets);
    fwrite(&base, 4, 1, g_packets);
    g_packets_start = GetTickCount();
    sv_log("packet capture: %s", path);
}

/* ------------------------------------------------------------ imports */

/* Each import of the game is a function of src/platform/plat_imports.c (Phase 5.2). */
void *sr_import(const char *dll, const char *name)
{
    void *p;
    if (!_stricmp(dll, "MSVCR100.dll") && !strcmp(name, "printf"))
        return (void *)sv_printf;
    p = plat_lookup(dll, name, 0);
    if (!p) {
        sv_log("import not found: %s!%s", dll, name);
        ExitProcess(1);
    }
    return p;
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    void *p = plat_lookup(dll, NULL, ordinal);
    if (!p) {
        sv_log("import not found: %s!#%u", dll, ordinal);
        ExitProcess(1);
    }
    /* WSOCK32 ordinal 17 is recvfrom and 20 is sendto. */
    if (g_packets && !_stricmp(dll, "WSOCK32.dll") && ordinal == 17) {
        *(void **)&g_recvfrom = p;
        return (void *)capture_recvfrom;
    }
    if (g_packets && !_stricmp(dll, "WSOCK32.dll") && ordinal == 20) {
        *(void **)&g_sendto = p;
        return (void *)capture_sendto;
    }
    return p;
}

/* The main loop calls this every minute. The hook build prints its per-hook profile here.
 * The standalone server prints the profiler of the game itself when SV_TIMERS is set: each
 * timer has a name at +0 and its RDTSC cycles of the last tick at +0x48 (stoptimer, RVA
 * 0xb3f0, adds to it, and the code that starts the timer sets it to 0 each tick). The
 * table is g_timer (RVA 0x14043a0, 256 records of 0x58 bytes). */
void sv_hooks_lifted_profile(void)
{
    int i;
    if (!getenv("SV_TIMERS"))
        return;
    for (i = 0; i < 256; i++) {
        const uint8_t *t = (const uint8_t *)sr_base + 0x14043a0 + i * 0x58;
        uint64_t last;
        if (!t[0])
            continue;
        memcpy(&last, t + 0x48, sizeof last);
        sv_log("  timer %-24.40s %8.2f Mcycles in the last tick", (const char *)t, last / 1e6);
    }
}

/* ------------------------------------------------------------ sampling profiler */

/* SV_SAMPLE=<file>: a thread reads the EIP of the main thread every millisecond and writes
 * one hex address per line. tools/lift/samples.py maps them to functions with the .map file. */
static HANDLE g_main_thread;
static FILE *g_samples;

static DWORD WINAPI sampler(void *arg)
{
    unsigned count = 0;
    (void)arg;
    timeBeginPeriod(1);
    for (;;) {
        CONTEXT c;
        Sleep(1);
        if (SuspendThread(g_main_thread) == (DWORD)-1)
            continue;
        c.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(g_main_thread, &c))
            fprintf(g_samples, "%08lx\n", c.Eip);
        ResumeThread(g_main_thread);
        if (++count % 1000 == 0)
            fflush(g_samples);  /* a stop with TerminateProcess loses at most one second */
    }
}

static void start_sampler(void)
{
    const char *path = getenv("SV_SAMPLE");
    if (!path || !(g_samples = fopen(path, "w")))
        return;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    fprintf(g_samples, "base %08lx\n", (unsigned long)(uintptr_t)GetModuleHandleA(NULL));
    CreateThread(NULL, 0, sampler, NULL, 0, NULL);
    sv_log("sampling profiler: %s", path);
}

/* ------------------------------------------------------------ image */

/* A region of SizeOfImage bytes, filled like the Windows loader fills the 024d: .rdata and
 * the initialized .data at their RVAs, the rest zero, and each image address moved by the
 * delta between the region and the file base 0x400000. */
static int map_image(void)
{
    unsigned i;
    uint8_t *base = VirtualAlloc(NULL, sr_image_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) {
        fprintf(stderr, "cannot allocate the image region (%u MiB, error %lu)\n", sr_image_size >> 20,
                GetLastError());
        return 0;
    }
    sr_base = (uintptr_t)base;
    memcpy(base + sr_image_rdata_rva, sr_image_rdata, sr_image_rdata_size);
    memcpy(base + sr_image_data_rva, sr_image_data, sr_image_data_size);
    for (i = 0; i < sr_image_reloc_count; i++)
        *(uint32_t *)(base + sr_image_relocs[i]) += (uint32_t)(sr_base - 0x400000u);
    return 1;
}

/* SV_DUMP=<file>: a minidump at a crash. SV_DUMP_TYPE=full adds all the memory (the image
 * region alone is about 600 MB). The small dump has the stacks and the memory near them. */
static void write_dump(EXCEPTION_POINTERS *ep)
{
    const char *path = getenv("SV_DUMP"), *type = getenv("SV_DUMP_TYPE");
    int full = type && !strcmp(type, "full");
    MINIDUMP_EXCEPTION_INFORMATION info;
    HANDLE f;
    BOOL ok;
    if (!path)
        return;
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        sv_log("CRASH: cannot write the dump %s", path);
        return;
    }
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = ep;
    info.ClientPointers = FALSE;
    ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                           (MINIDUMP_TYPE)(full ? MiniDumpWithFullMemory | MiniDumpWithHandleData |
                                                      MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules
                                                : MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                                                      MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
                           &info, NULL, NULL);
    CloseHandle(f);
    sv_log(ok ? "CRASH: %s dump in %s" : "CRASH: the %s dump failed (%s)", full ? "full" : "small", path);
}

/* A crash: write where it happened, so that a test log is enough to find the line. The
 * address is given as module+offset (build/hook/subvulpes-server.map and .pdb), and as
 * an image RVA when it is inside the 024d region (a game address used as code or data). */
static LONG WINAPI on_crash(EXCEPTION_POINTERS *ep)
{
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t at = (uintptr_t)er->ExceptionAddress;
    HMODULE m = NULL;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (const char *)at, &m))
        GetModuleFileNameA(m, name, sizeof name);
    sv_log("CRASH: exception %08lx at %s+0x%x, data address %08x", er->ExceptionCode,
           strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name, (unsigned)(at - (uintptr_t)m),
           er->NumberParameters >= 2 ? (unsigned)er->ExceptionInformation[1] : 0);
    if (er->NumberParameters >= 2 && er->ExceptionInformation[1] >= sr_base &&
        er->ExceptionInformation[1] < sr_base + sr_image_size)
        sv_log("CRASH: the data address is image RVA 0x%x", (unsigned)(er->ExceptionInformation[1] - sr_base));
    if (g_packets)
        fflush(g_packets);
    if (g_samples)
        fflush(g_samples);
    write_dump(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* SV_TEST_CRASH=<seconds>: a write to address 0 after that time, to test the crash log and
 * the dump of a launcher preset. */
static DWORD WINAPI test_crash(void *arg)
{
    Sleep((DWORD)(uintptr_t)arg * 1000);
    *(volatile int *)0 = 0;
    return 0;
}

int main(void)
{
    unsigned cw;
    open_log();
    SetUnhandledExceptionFilter(on_crash);
    if (!map_image())
        return 1;
    sr_fn_init();
    /* The CRT of the 024d sets 53-bit x87 precision at start-up (_setdefaultprecision). */
    _controlfp_s(&cw, _PC_53, _MCW_PC);
    lf_x87_init();
    sv_log("standalone server: image at %p", (void *)sr_base);
    open_packets();
    start_sampler();
    if (getenv("SV_TEST_CRASH"))
        CreateThread(NULL, 0, test_crash, (void *)(uintptr_t)atoi(getenv("SV_TEST_CRASH")), 0, NULL);
    sr_getpaths();  /* never returns: it runs the main loop */
    return 0;
}
