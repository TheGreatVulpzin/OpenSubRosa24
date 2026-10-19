/* The profiler timers of the game: each section of the tick adds its rdtsc cycles to a Timer.
 * TimerList holds the index of the timer of each section. See profiler.c for stoptimer. */
#ifndef SR_PROFILER_H
#define SR_PROFILER_H

#include <intrin.h>

#include "sr_runtime.h"

#define g_timer SR_AT(Timer, 0x14043a0)
#define g_timerlist (*SR_AT(TimerList, 0x12a81c60))

/* stoptimer has no clean version yet: the call goes to the lifted C in profiler.c. */
void __cdecl lf_0b3f0(int timer);
#define stoptimer lf_0b3f0

/* Give a profiler timer its name and clear its total. */
static __inline void nametimer(int timer, const char *name)
{
    Timer *t = &g_timer[timer];
    char *d = t->name;
    while ((*d++ = *name++) != '\0')
        ;
    t->total_lo = 0;
    t->total_hi = 0;
    t->ms = 0;
}

static __inline void starttimer(int timer)
{
    unsigned __int64 now = __rdtsc();
    g_timer[timer].start_lo = (int)now;
    g_timer[timer].start_hi = (int)(now >> 0x20);
}

#endif /* SR_PROFILER_H */
