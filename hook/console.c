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
#include <stdlib.h>
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

/* Columns of the console window. */
static int console_width(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    const char *env = getenv("SV_UI_WIDTH");
    if (env && atoi(env) > 0)
        return atoi(env);
    if (GetConsoleScreenBufferInfo(g_out, &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 80;
}

/* ASCII art from asciiart.eu, ASCII only so the columns line up in any console font.
 * Wide window: running fox by H P Barmario (Morfina), signed "mrf" in the art.
 * Narrow window: small fox by "cp97". */
static void banner(void)
{
    static const char *const wide[] = {
        "   /|_/|",
        "  / ^ ^(_o",
        " /    __.'",
        " /     \\",
        "(_) (_) '._",
        "  '.__     '. .-''-'.",
        "     ( '.   ('.____.''",
        "     _) )'_, )mrf",
        "    (__/ (__/",
    };
    static const char *const wide_side[] = {"", "SubVulpes", "Sub Rosa 024d server", "", "", "", "", "", ""};
    static const char *const narrow[] = {" ^...^", "<_* *_>", "  \\_/"};
    static const char *const narrow_side[] = {"", "SubVulpes", ""};
    /* "SubVulpes" in the figlet font "small" (39 columns), drawn beside rows 2 to 6 of the fox. */
    static const char *const big[] = {
        " ___      _  __   __    _",
        "/ __|_  _| |_\\ \\ / /  _| |_ __  ___ ___",
        "\\__ \\ || | '_ \\ V / || | | '_ \\/ -_|_-<",
        "|___/\\_,_|_.__/\\_/ \\_,_|_| .__/\\___/__/",
        "                         |_|",
    };
    int width = console_width();
    const char *const *art = width >= 52 ? wide : narrow;
    int i, rows = width >= 52 ? 9 : 3, pad = width >= 52 ? 28 : 10;
    char line[160];
    write_text("\n");
    for (i = 0; i < rows; i++) {
        const char *text;
        if (width >= 72 && i >= 1 && i <= 5)
            text = big[i - 1];
        else if (width >= 72 && i == 7)
            text = "     Sub Rosa 024d server";
        else if (width >= 52)
            text = (i == 1 || i == 2) ? wide_side[i] : "";
        else
            text = narrow_side[i];
        snprintf(line, sizeof line, "%s%-*s%s%s\n", g_color ? ORANGE : "", pad, art[i], g_color ? RESET : "", text);
        write_text(line);
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
