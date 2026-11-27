/* Scenario forcer: writes chosen values into the game memory at chosen game ticks.
 *
 * It makes a recording reach states that a short test does not reach: a round that ends in a few
 * seconds, missions that appear at once, a world clock that is near its next period. It runs on the
 * game thread, between two calls of the game, so a write never lands in the middle of a function.
 *
 * The file subvulpes_force.txt in the folder of the executable switches it on. No file, no change.
 * One rule on each line. Text after # is a comment.
 *
 *   ticks <rva>                                  the RVA of the 4 byte tick counter of the game
 *   at <tick> [if <rva> <size> <value>]... set <rva> <size> <value>
 *   every <ticks> [from <tick>] [if <rva> <size> <value>]... set <rva> <size> <value>
 *
 * "at" fires one time, when the tick counter reaches <tick>. "every" fires as soon as its conditions
 * hold, then again after each period. "from" delays the first fire of each round: the game sets the
 * counter back to 0 at each new round, and some events happen in its first ticks. A rule can have up to
 * two "if" conditions: it waits until all hold. The game thread checks the rules only when a hooked
 * function runs, so a condition on a value that holds for one tick can be missed: use a state. The size is 1, 2 or 4 bytes. Numbers are decimal or 0x hex. A float goes in as its bit
 * pattern (tools: instrumentation/recording/force.py).
 *
 * A write after the snapshot is not part of the snapshot. The harness compares the original code with
 * the clean code on the same state, so this does not cause a false difference.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

extern uintptr_t sr_base;

#define RULE_MAX 64
#define COND_MAX 2

typedef struct ForceCond {
    uint32_t rva, value;
    int size;
} ForceCond;

typedef struct ForceRule {
    int every;          /* 0: "at", 1: "every" */
    uint32_t tick;      /* "at": the tick. "every": the period */
    uint32_t from;      /* "every": the first tick of each round that can fire */
    uint32_t next;      /* the tick of the next fire */
    int num_conds;
    ForceCond conds[COND_MAX];
    uint32_t rva, value;
    int size;
    int done;
    int fires;
} ForceRule;

static ForceRule g_rules[RULE_MAX];
static int g_count, g_loaded;
static uint32_t g_ticks_rva = 0x90dc578u; /* game.ticks of the 024d server */
static uint32_t g_next_check, g_last_ticks;

static int valid_size(int size)
{
    return size == 1 || size == 2 || size == 4;
}

/* Read the value of 1, 2 or 4 bytes. Returns 0 when the address cannot be read. */
static int read_value(uint32_t rva, int size, uint32_t *out)
{
    const uint8_t *p = (const uint8_t *)(sr_base + rva);
    uint32_t v = 0;
    __try {
        memcpy(&v, p, (size_t)size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    *out = v;
    return 1;
}

static int write_value(uint32_t rva, int size, uint32_t value)
{
    uint8_t *p = (uint8_t *)(sr_base + rva);
    __try {
        memcpy(p, &value, (size_t)size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 1;
}

/* Split a line into words. Returns the number of words. */
static int split_words(char *line, char **words, int max)
{
    int n = 0;
    char *hash = strchr(line, '#');
    char *p;
    if (hash)
        *hash = 0;
    for (p = strtok(line, " \t\r\n"); p && n < max; p = strtok(NULL, " \t\r\n"))
        words[n++] = p;
    return n;
}

static uint32_t number(const char *text)
{
    return (uint32_t)strtoul(text, NULL, 0);
}

/* Parse one rule. Returns 1 when the line holds a rule or the tick address. */
static int parse_line(char *line, int line_no)
{
    char *w[16];
    int n = split_words(line, w, 16), i = 1;
    ForceRule *r;
    if (n == 0)
        return 1;
    if (strcmp(w[0], "ticks") == 0 && n == 2) {
        g_ticks_rva = number(w[1]);
        return 1;
    }
    if ((strcmp(w[0], "at") != 0 && strcmp(w[0], "every") != 0) || n < 6 || g_count >= RULE_MAX) {
        sv_log("ERROR force: line %d is not a rule", line_no);
        return 0;
    }
    r = &g_rules[g_count];
    memset(r, 0, sizeof *r);
    r->every = strcmp(w[0], "every") == 0;
    r->tick = number(w[1]);
    i = 2;
    if (r->every && i + 1 < n && strcmp(w[i], "from") == 0) {
        r->from = number(w[i + 1]);
        i += 2;
    }
    while (i + 3 < n && strcmp(w[i], "if") == 0) {
        ForceCond *c;
        if (r->num_conds >= COND_MAX) {
            sv_log("ERROR force: line %d has too many conditions", line_no);
            return 0;
        }
        c = &r->conds[r->num_conds++];
        c->rva = number(w[i + 1]);
        c->size = (int)number(w[i + 2]);
        c->value = number(w[i + 3]);
        if (!valid_size(c->size)) {
            sv_log("ERROR force: line %d has a wrong size in a condition", line_no);
            return 0;
        }
        i += 4;
    }
    if (i + 3 >= n || strcmp(w[i], "set") != 0) {
        sv_log("ERROR force: line %d has no set", line_no);
        return 0;
    }
    r->rva = number(w[i + 1]);
    r->size = (int)number(w[i + 2]);
    r->value = number(w[i + 3]);
    if (!valid_size(r->size) || (r->every && r->tick == 0)) {
        sv_log("ERROR force: line %d has a wrong size or period", line_no);
        return 0;
    }
    /* A repeating rule fires as soon as its conditions hold, after its "from" tick. The period only
     * spaces the repeats. */
    r->next = r->every ? r->from : r->tick;
    g_count++;
    return 1;
}

static void load_rules(void)
{
    char path[MAX_PATH], line[256];
    FILE *f;
    int line_no = 0, bad = 0;
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    char *slash;
    g_loaded = 1;
    if (n == 0 || n >= sizeof path)
        return;
    slash = strrchr(path, '\\');
    if (!slash)
        return;
    strcpy(slash + 1, "subvulpes_force.txt");
    f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        if (!parse_line(line, line_no))
            bad++;
    }
    fclose(f);
    sv_log("force: %d rules from subvulpes_force.txt (%d bad lines), tick counter at rva 0x%x", g_count, bad, g_ticks_rva);
    sv_console_event(0, "scenario forcer: %d rules", g_count);
}

static int conditions_hold(const ForceRule *r)
{
    int k;
    for (k = 0; k < r->num_conds; k++) {
        uint32_t current;
        if (!read_value(r->conds[k].rva, r->conds[k].size, &current) || current != r->conds[k].value)
            return 0;
    }
    return 1;
}

static void force_rules(void);

/* Called by the recorder before each outer call of the game, on the game thread. A second thread
 * that calls a hooked function at the same time skips the check. */
void sv_force_check(void)
{
    static volatile LONG busy;
    if (g_loaded && g_count == 0)
        return;
    if (InterlockedExchange(&busy, 1))
        return;
    if (!g_loaded)
        load_rules();
    if (g_count > 0)
        force_rules();
    InterlockedExchange(&busy, 0);
}

static void force_rules(void)
{
    uint32_t ticks;
    int i;
    if (!read_value(g_ticks_rva, 4, &ticks))
        return;
    if (ticks < g_last_ticks) {
        /* The game set the counter back (resetgame does this at each round). A repeating rule
         * waits for its condition again from the start. */
        for (i = 0; i < g_count; i++)
            if (g_rules[i].every)
                g_rules[i].next = g_rules[i].from;
        g_next_check = 0;
    }
    g_last_ticks = ticks;
    if (ticks < g_next_check)
        return;
    g_next_check = 0xffffffffu;
    for (i = 0; i < g_count; i++) {
        ForceRule *r = &g_rules[i];
        if (r->done)
            continue;
        if (ticks >= r->next) {
            if (!conditions_hold(r)) {
                /* A condition may hold in the next tick. */
                if (ticks + 1 < g_next_check)
                    g_next_check = ticks + 1;
                continue;
            }
            if (write_value(r->rva, r->size, r->value)) {
                r->fires++;
                sv_log("force: tick %u wrote %u bytes at rva 0x%x = 0x%x (rule %d)", ticks, r->size, r->rva, r->value, i + 1);
            } else {
                sv_log("ERROR force: tick %u cannot write rva 0x%x (rule %d)", ticks, r->rva, i + 1);
                r->done = 1;
                continue;
            }
            if (r->every)
                r->next = ticks + r->tick;
            else
                r->done = 1;
        }
        if (!r->done && r->next < g_next_check)
            g_next_check = r->next;
    }
}
