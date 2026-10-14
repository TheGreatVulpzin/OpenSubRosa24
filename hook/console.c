/* The SubVulpes part of the server console (the conhost window of the 024d).
 *
 * - A banner and the active mode when the server starts.
 * - The window title shows the mode and the recording size, updated every 2 seconds.
 * - One [SubVulpes] line for important events (hooks installed, snapshot, errors) and a
 *   summary every 5 minutes. Never one line per call: console writes are slow.
 *
 * The game prints to the same console. These lines go straight to the console handle.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hook.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE g_out;
static int g_on, g_color;
static char g_mode[64];
static DWORD g_start;

static const char *ORANGE = "\x1b[38;5;208m", *RED = "\x1b[91m", *GRAY = "\x1b[90m", *RESET = "\x1b[0m";

static void write_text(const char *s)
{
    DWORD n;
    WriteConsoleA(g_out, s, (DWORD)strlen(s), &n, NULL);
}

/* One line: "[SubVulpes] text", orange (red for an error). */
void sv_console_event(int error, const char *fmt, ...)
{
    char text[512], line[640];
    va_list ap;
    if (!g_on)
        return;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    if (g_color)
        snprintf(line, sizeof line, "%s[SubVulpes]%s %s%s%s\n", ORANGE, RESET, error ? RED : "", text, RESET);
    else
        snprintf(line, sizeof line, "[SubVulpes] %s\n", text);
    write_text(line);
}

static void banner(void)
{
    static const char *const art[] = {
        "  /\\_/\\   ____        _ __     __     _",
        " ( o.o ) / ___| _   _| |\\ \\   / /   _| |_ __   ___  ___",
        "  > ^ <  \\___ \\| | | | '_ \\ \\ / / | | | | '_ \\ / _ \\/ __|",
        "          ___) | |_| | |_) \\ V /| |_| | | |_) |  __/\\__ \\",
        "         |____/ \\__,_|_.__/ \\_/  \\__,_|_| .__/ \\___||___/",
        "                                        |_|   Sub Rosa 024d",
    };
    size_t i;
    write_text("\n");
    for (i = 0; i < sizeof art / sizeof art[0]; i++) {
        if (g_color)
            write_text(ORANGE);
        write_text(art[i]);
        if (g_color)
            write_text(RESET);
        write_text("\n");
    }
    write_text("\n");
}

static void format_mb(char *out, size_t n, long long bytes)
{
    snprintf(out, n, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
}

/* Title every 2 seconds, summary every 5 minutes. */
static DWORD WINAPI status_thread(LPVOID unused)
{
    DWORD last_summary = GetTickCount();
    (void)unused;
    for (;;) {
        int calls;
        long long bytes;
        char title[256], mb[32];
        Sleep(2000);
        sv_record_stats(&calls, &bytes);
        format_mb(mb, sizeof mb, bytes);
        snprintf(title, sizeof title, "Sub Rosa 024d + SubVulpes | %s | %d calls recorded, %s", g_mode, calls, mb);
        SetConsoleTitleA(title);
        if (GetTickCount() - last_summary >= 5 * 60 * 1000) {
            DWORD up = (GetTickCount() - g_start) / 1000;
            last_summary = GetTickCount();
            sv_console_event(0, "%s%02lu:%02lu:%02lu%s  %d calls recorded, %s", g_color ? GRAY : "", up / 3600,
                             up / 60 % 60, up % 60, g_color ? RESET : "", calls, mb);
        }
    }
}

/* Call once, after the hooks are installed. mode: a short text such as "recording". */
void sv_console_init(const char *mode, int hooks)
{
    DWORD m;
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_out == NULL || g_out == INVALID_HANDLE_VALUE || !GetConsoleMode(g_out, &m))
        return;  /* no console */
    g_color = SetConsoleMode(g_out, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    g_on = 1;
    g_start = GetTickCount();
    strncpy(g_mode, mode, sizeof g_mode - 1);
    banner();
    sv_console_event(0, "mode: %s", mode);
    sv_console_event(0, "%d hooks installed. Log: subvulpes.log", hooks);
    CloseHandle(CreateThread(NULL, 0, status_thread, NULL, 0, NULL));
}
