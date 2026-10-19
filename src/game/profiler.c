/* stoptimer (RVA 0xb3f0): the end of a profiler timer. It adds the RDTSC cycles since the
 * start of the timer to its total, and writes the total in milliseconds.
 *
 * The physics loops call it many times in each tick, so it must be fast. The math gives
 * the same bits as the 024d on the x87 (53-bit precision).
 */
#include "profiler.h"

#include <stdint.h>

#define g_sysinfo (*SR_AT(SysInfo, 0x916d2e0))

void stoptimer(int timer)
{
    Timer *t = &g_timer[timer];
    uint64_t now = __rdtsc();
    uint64_t start = (uint64_t)(uint32_t)t->start_hi << 32 | (uint32_t)t->start_lo;
    uint64_t total = ((uint64_t)(uint32_t)t->total_hi << 32 | (uint32_t)t->total_lo) + (now - start);
    double cycles, ms;
    int64_t whole;

    t->total_lo = (int)(uint32_t)total;
    t->total_hi = (int)(uint32_t)(total >> 32);
    if (total >> 63) {
        cycles = 9223372036854775808.0 + (double)(int64_t)(total & 0x7fffffffffffffffull);
    } else {
        /* The count is rounded to a double before the multiply. Without the store, the x87
         * keeps all 64 bits and the result can change by 1. */
        volatile double rounded = (double)(int64_t)total;
        cycles = rounded;
    }
    ms = cycles * g_sysinfo.ms_per_tick;
    /* fisttp truncates with no change to the control word: the same result as
     * trunc and _ftol2, at a lower cost. */
    __asm {
        fld ms
        fisttp whole
    }
    t->ms = (int)(uint32_t)whole;
}

