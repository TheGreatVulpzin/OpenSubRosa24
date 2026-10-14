/* Console output of the harness (harness_ui.h). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "harness_ui.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static int g_color;

/* Colors (ANSI escape codes). Empty strings when stdout is not a console. */
static const char *c_orange = "", *c_green = "", *c_red = "", *c_yellow = "", *c_magenta = "", *c_gray = "",
                  *c_bold = "", *c_reset = "";

/* The start message: one line, chosen at random. Written by the project owner. */
static const char *const g_quotes[] = {
    "Basicamente o Subrosa de Theseus",
    "Usando a informação suculenta do parasita .dll no executavel do subrosa",
    "Por que subvulpes? Só honrando uma lore antiga minha... - Vulpzin",
    "Tudo isso começou por causa do Murilo querendo fazer um mod manager",
    "Valeu Claude",
    "Claude é legal, mas a quantidade de handholding é complicada.",
    "Isso é meio assustador, sabia",
    "Jamais deixe o Sqwimble em números negativos.",
    "Você sabia que trens são ótimos chapéus",
    "davy jones",
    "Faça uma call de faça um dirigivel no trem... Subrosa",
    "Maria vai com as outras",
    "Não faço ideia cara",
    "and now your host for this evening, the rolling stones",
};

void ui_init(void)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode))
        return;  /* redirected: plain text */
    SetConsoleOutputCP(CP_UTF8);
    if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return;
    g_color = 1;
    c_orange = "\x1b[38;5;208m";
    c_green = "\x1b[32m";
    c_red = "\x1b[91m";
    c_yellow = "\x1b[33m";
    c_magenta = "\x1b[35m";
    c_gray = "\x1b[90m";
    c_bold = "\x1b[1m";
    c_reset = "\x1b[0m";
}

void ui_banner(void)
{
    static const char *const art[] = {
        "  /\\_/\\   ____        _ __     __     _",
        " ( o.o ) / ___| _   _| |\\ \\   / /   _| |_ __   ___  ___",
        "  > ^ <  \\___ \\| | | | '_ \\ \\ / / | | | | '_ \\ / _ \\/ __|",
        "          ___) | |_| | |_) \\ V /| |_| | | |_) |  __/\\__ \\",
        "         |____/ \\__,_|_.__/ \\_/  \\__,_|_| .__/ \\___||___/",
        "                                        |_|   differential harness",
    };
    LARGE_INTEGER t;
    size_t i, n = sizeof g_quotes / sizeof g_quotes[0];
    QueryPerformanceCounter(&t);
    printf("\n");
    for (i = 0; i < sizeof art / sizeof art[0]; i++)
        printf("%s%s%s%s\n", c_orange, c_bold, art[i], c_reset);
    printf("\n  %s\"%s\"%s\n\n", c_gray, g_quotes[(size_t)(t.QuadPart ^ time(NULL)) % n], c_reset);
}

void ui_info(const char *label, const char *fmt, ...)
{
    va_list ap;
    printf("  %s%-12s%s ", c_orange, label, c_reset);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void ui_warning(const char *fmt, ...)
{
    va_list ap;
    printf("  %sWARNING%s ", c_yellow, c_reset);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* "[ 12/316] name   24 same   0 different   0 crashed   0 skipped  note" */
void ui_function(int index, int total, const char *name, int same, int diff, int crash, int skip, int known,
                 const char *note)
{
    const char *diff_color = diff ? (known ? c_yellow : c_red) : c_gray;
    printf("%s[%3d/%d]%s %-34s %s%3d same%s %s%3d different%s %s%3d crashed%s %s%3d skipped%s  %s%s%s\n", c_gray,
           index, total, c_reset, name, same ? c_green : c_gray, same, c_reset, diff_color, diff, c_reset,
           crash ? c_magenta : c_gray, crash, c_reset, c_gray, skip, c_reset,
           diff && !known ? c_red : known ? c_yellow : crash ? c_magenta : "", note, c_reset);
}

void ui_summary(int tested, int same, int diff, int known, int crash, int skip, double seconds)
{
    const char *line = "  +----------------------------------------------------------------------+";
    int ok = !diff && !crash;
    int min = (int)(seconds / 60), sec = (int)seconds % 60;
    printf("\n%s%s%s\n", c_orange, line, c_reset);
    printf("  %s|%s  %s%4d%s functions   %s%5d same%s   %s%3d different%s   %s%3d known%s   %s%3d crashed%s   %4d skipped\n",
           c_orange, c_reset, c_bold, tested, c_reset, c_green, same, c_reset, diff ? c_red : c_gray, diff, c_reset,
           known ? c_yellow : c_gray, known, c_reset, crash ? c_magenta : c_gray, crash, c_reset, skip);
    printf("  %s|%s  result: %s%s%s   time: %dm %02ds\n", c_orange, c_reset, ok ? c_green : c_red,
           ok ? "OK, the lifted code does the same as the original" : "DIFFERENT, check the lines in red", c_reset,
           min, sec);
    printf("%s%s%s\n\n", c_orange, line, c_reset);
    /* The plain line for scripts (tools and the CI read it). */
    printf("%d functions tested: %d calls same, %d different, %d known differences, %d lifted crashed, "
           "%d skipped\n",
           tested, same, diff, known, crash, skip);
}
