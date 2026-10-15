/* Generic verification: run the new function and the original, then compare memory.
 *
 * During each run, the game .data section is read-only. Each write raises an
 * exception: the verifier keeps a copy of the page before the write and unlocks the page.
 * At the end it knows which pages each version changed and compares only those.
 *
 * Flow of a hook (see SV_HOOK_VOID in hook_wrap.h):
 *   sv_verify_should(v)              decide if this call is verified (sampling)
 *   sv_track_begin_new(v)            new version runs
 *   sv_track_switch_to_original()    original runs
 *   sv_verify_end(v, ...)            compare, write to the log, keep the result of the original
 *
 * Limits: covers the .data section of the executable (all game globals) and, with
 * sv_verify_watch_heap, the MSVCR100 heap. Writes to the stack or DLLs are not covered. The server must run on one thread.
 */
#ifndef SV_VERIFY_H
#define SV_VERIFY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SvVerify {
    const char *name;
    long every;      /* verify 1 of every N calls. Negative value = hook default;
                        subvulpes.ini verify_every.<name> has priority. */
    long calls;      /* total calls */
    long checks;     /* verified calls */
    long fails;      /* verifications with a difference */
    int broken;      /* 1 = the new version crashed; the hook uses only the original from now on */
    int crashed;     /* 1 = the new version crashed in this call */
    /* Learned scope: page ranges of .data that the function usually writes.
     * Outside full rounds, only these ranges are protected. */
    unsigned nranges;
    unsigned range[32][2];   /* [first page, last page + 1) inside .data */
    int recheck;             /* 1 = the next verification is a full round */
    long unconfirmed;        /* differences in scope rounds (not counted in fails) */
    long near_floats;        /* float words that differ only in the last bits (not counted) */
} SvVerify;

/* Tests only: use another section of the executable instead of .data. Call before sv_verify_init. */
void sv_verify_set_section(const char *name);

/* Find the .data section and the MSVCR100 rand() state. Returns 0 on failure. */
int sv_verify_init(void);

/* Also verify the writes to the MSVCR100 heap (malloc blocks of the game) by copy and compare.
 * Call after sv_verify_init. subvulpes.ini verify_heap=0 turns it off. Returns 1 when active. */
int sv_verify_watch_heap(void);

/* Do not compare this range of .data (rva, size in bytes). For memory that holds values
 * that change on each run, such as the profiler timers (CPU cycle counter). The original
 * result stays in memory as usual. */
void sv_verify_ignore(uint32_t rva, uint32_t size);

/* The .data section that the verifier watches. Returns 0 before sv_verify_init. */
int sv_verify_data(const uint8_t **data, size_t *size);

/* 1 = a verification is running now (a hook called from inside it must run the original). */
int sv_verify_busy(void);

/* subvulpes.ini original_only=1: every hook runs only the original function (no C version,
 * no verification). The thunks still record arguments: a recording session for the
 * offline test runs exactly like the original server. */
int sv_original_only(void);

/* Two float results are the same: equal, both NaN, or at most verify_float_ulps apart. */
int sv_same_float(double a, double b);

/* 1 = verify this call. Obeys verify=0 and sampling. */
int sv_verify_should(SvVerify *v);

/* Start to record writes. Call before the new version.
 * In the first verifications and every verify_full_every (default 50), protect the whole
 * .data and learn the written globals. In the others, protect only the learned scope. */
void sv_track_begin_new(SvVerify *v);

/* Keep the result of the new version, undo its writes and start to record again.
 * Call between the new version and the original. */
void sv_track_switch_to_original(void);

/* Stop recording, compare, and keep the result of the original.
 * same_return: 1 if the return values are equal (or if the function returns void).
 * detail: short text for the log (can be NULL). */
void sv_verify_end(SvVerify *v, int same_return, const char *detail);

/* SEH filter for the new version: log the crash, mark the hook as broken
 * and tell Windows to run the __except block. */
int sv_crash_filter(SvVerify *v, struct _EXCEPTION_POINTERS *ep);

/* Remember a hook call (ring of 16) for the crash report. */
void sv_note_call(const char *name);

/* Log the address and the last hooks when the process dies of an exception. */
void sv_install_crash_report(void);

/* Summary of one hook in the log. */
void sv_verify_summary(const SvVerify *v);

#ifdef __cplusplus
}
#endif

#endif /* SV_VERIFY_H */
