/* Console output of the harness (harness_ui.h). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
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

/* Columns of the console window; 80 when stdout is redirected. */
static int console_width(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 80;
}

/* Prints the text in quotes, indented by two spaces, breaking lines at spaces to fit the width. */
static void print_wrapped(const char *text, int width)
{
    int room = width - 4 > 20 ? width - 4 : 20;
    int col = 1;
    const char *p = text;
    printf("  %s\"", c_gray);
    while (*p) {
        const char *e = strchr(p, ' '), *q;
        int len = 0;
        if (!e)
            e = p + strlen(p);
        for (q = p; q < e; q++)
            len += ((*q & 0xC0) != 0x80);  /* count UTF-8 characters, not bytes */
        if (col + len > room && col > 1) {
            printf("\n   ");
            col = 1;
        }
        printf("%.*s", (int)(e - p), p);
        col += len;
        if (*e == ' ') {
            printf(" ");
            col++;
            e++;
        }
        p = e;
    }
    printf("\"%s\n", c_reset);
}

void ui_banner(void)
{
    /* ASCII art from asciiart.eu, ASCII only so the columns line up in any font.
     * Wide: running fox by H P Barmario (Morfina), signed "mrf" in the art.
     * Narrow: small fox by "cp97". */
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
    static const char *const wide_side[] = {"", "SubVulpes", "differential harness", "Sub Rosa 024d", "", "", "", "", ""};
    static const char *const narrow[] = {" ^...^", "<_* *_>", "  \\_/"};
    static const char *const narrow_side[] = {"", "SubVulpes harness", ""};
    LARGE_INTEGER t;
    int i, width = console_width();
    size_t n = sizeof g_quotes / sizeof g_quotes[0];
    QueryPerformanceCounter(&t);
    printf("\n");
    if (width >= 52) {
        for (i = 0; i < 9; i++)
            printf("%s%s%-28s%s%s%s%s\n", c_orange, c_bold, wide[i], c_reset, c_bold, wide_side[i], c_reset);
    } else {
        for (i = 0; i < 3; i++)
            printf("%s%s%-10s%s%s%s%s\n", c_orange, c_bold, narrow[i], c_reset, c_bold, narrow_side[i], c_reset);
    }
    printf("\n");
    print_wrapped(g_quotes[(size_t)(t.QuadPart ^ time(NULL)) % n], width);
    printf("\n");
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

/* Wide: "[ 12/316] name   24 same   0 different   0 crashed   0 skipped  note"
 * Narrow: "[ 12/316] name  24 ok" plus the counts that are not zero (no note). */
void ui_function(int index, int total, const char *name, int same, int diff, int crash, int skip, int known,
                 const char *note)
{
    const char *diff_color = diff ? (known ? c_yellow : c_red) : c_gray;
    int width = console_width();
    if (width >= 96) {
        printf("%s[%3d/%d]%s %-34s %s%3d same%s %s%3d different%s %s%3d crashed%s %s%3d skipped%s  %s%s%s\n", c_gray,
               index, total, c_reset, name, same ? c_green : c_gray, same, c_reset, diff_color, diff, c_reset,
               crash ? c_magenta : c_gray, crash, c_reset, c_gray, skip, c_reset,
               diff && !known ? c_red : known ? c_yellow : crash ? c_magenta : "", note, c_reset);
    } else {
        char head[24], counts[96];
        int hl, cl = 0, room;
        snprintf(head, sizeof head, "[%3d/%d] ", index, total);
        hl = (int)strlen(head);
        if (diff)
            cl += snprintf(counts + cl, sizeof counts - cl, " %d diff", diff);
        if (crash)
            cl += snprintf(counts + cl, sizeof counts - cl, " %d crash", crash);
        if (skip)
            cl += snprintf(counts + cl, sizeof counts - cl, " %d skip", skip);
        counts[cl] = 0;
        room = width - hl - 8 - cl;  /* 8 = "  NNN ok" */
        if (room < 8)
            room = 8;
        printf("%s%s%s%-*.*s  %s%3d ok%s", c_gray, head, c_reset, room, room, name, same ? c_green : c_gray, same, c_reset);
        if (cl)
            printf("%s%s%s", diff ? diff_color : crash ? c_magenta : c_gray, counts, c_reset);
        printf("\n");
    }
}

void ui_summary(int tested, int same, int diff, int known, int crash, int skip, double seconds)
{
    char line[80];
    int width = console_width();
    int ok = !diff && !crash;
    int min = (int)(seconds / 60), sec = (int)seconds % 60;
    int w = width - 4 > 72 ? 72 : width - 4;
    if (w < 20)
        w = 20;
    memset(line, '-', (size_t)w);
    line[w] = 0;
    printf("\n  %s%s%s\n", c_orange, line, c_reset);
    if (width >= 90) {
        printf("  %s%4d%s functions   %s%5d same%s   %s%3d different%s   %s%3d known%s   %s%3d crashed%s   %4d skipped\n",
               c_bold, tested, c_reset, c_green, same, c_reset, diff ? c_red : c_gray, diff, c_reset,
               known ? c_yellow : c_gray, known, c_reset, crash ? c_magenta : c_gray, crash, c_reset, skip);
    } else {
        printf("  %s%5d%s functions   %s%5d%s same\n", c_bold, tested, c_reset, c_green, same, c_reset);
        printf("  %s%5d%s different   %s%5d%s known\n", diff ? c_red : c_gray, diff, c_reset, known ? c_yellow : c_gray,
               known, c_reset);
        printf("  %s%5d%s crashed     %5d skipped\n", crash ? c_magenta : c_gray, crash, c_reset, skip);
    }
    printf("  %s%s%s  %s%dm %02ds%s\n", ok ? c_green : c_red,
           ok ? "OK, same as the original" : "DIFFERENT, see the red lines", c_reset, c_gray, min, sec, c_reset);
    printf("  %s%s%s\n\n", c_orange, line, c_reset);
    /* The plain line for scripts (tools and the CI read it). */
    printf("%d functions tested: %d calls same, %d different, %d known differences, %d lifted crashed, "
           "%d skipped\n",
           tested, same, diff, known, crash, skip);
}
