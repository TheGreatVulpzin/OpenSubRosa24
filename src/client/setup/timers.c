/* The profiler timers of the client. A timer has a name, the processor cycle count at its
 * start and a total.
 */
#include <intrin.h>
#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define g_timer sr_data_timer

/* Give a timer its name and clear its total. The milliseconds keep their value. */
void __cdecl resettimer(int timer, const char *name)
{
    Timer *t = &g_timer[timer];

    strcpy(t->name, name);
    t->total_lo = 0;
    t->total_hi = 0;
}

/* Store the processor cycle count as the start of a timer. */
void __cdecl starttimer(int timer)
{
    unsigned __int64 now = __rdtsc();

    g_timer[timer].start_lo = (int)now;
    g_timer[timer].start_hi = (int)(now >> 32);
}
