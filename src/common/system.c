/* The clock, the process and the log files of the game. */
#include "system.h"

#include <string.h>

#include "sr_os.h"
#include "sr_runtime.h"

#define g_sysinfo (*SR_AT(SysInfo, SR_RVA_SYSINFO))
#define g_debug (*SR_AT(Debug, SR_RVA_DEBUG))
#define g_debugit (*SR_AT(int, 0x204020))
#define g_slogfp (*SR_AT(void *, 0x10348524))
#define g_tzset_done (*SR_AT(int, 0x61840))
#define g_numofaccounts (*SR_AT(int, SR_RVA_ACCOUNT + 4))
#define g_accounts ((Account *)(sr_base + SR_RVA_ACCOUNT + 8))

/* The offset between the Windows file time and the Unix time, in microseconds. The code
 * subtracts it from a count of 100 ns steps, so the seconds that it gives are far too big. */
#define EPOCH_OFFSET 11644473600000000ull

void setcpuaffinity(void)
{
    Debug *d = &g_debug;
    uintptr_t process_mask, system_mask;

    if (os_getprocessaffinitymask(os_getcurrentprocess(), &process_mask, &system_mask)) {
        d->affinity_before = (int)process_mask + 10000;
        if (process_mask & 1) {
            if (os_setprocessaffinitymask(os_getcurrentprocess(), 1))
                d->affinity_set = 1234;
            if (os_getprocessaffinitymask(os_getcurrentprocess(), &process_mask, &system_mask))
                d->affinity_after = (int)process_mask + 10000;
        }
    }
}

/* A 64-bit unsigned value as a double. A value with the top bit set is split so that the
 * top bit is added as 2^63. */
static double u64_to_double(uint64_t v)
{
    double top = (v >> 63) ? 9223372036854775808.0 : 0.0;
    return top + (double)(int64_t)(v & 0x7fffffffffffffffull);
}

void getcpufrequency(void)
{
    SysInfo *si = &g_sysinfo;
    int64_t counted;

    si->frequency[0] = 1000000000;
    si->frequency[1] = 0;
    if (os_qpf(&counted)) {
        si->frequency[0] = (uint32_t)counted;
        si->frequency[1] = (uint32_t)((uint64_t)counted >> 32);
    }
    if (si->frequency[1] == 0 && si->frequency[0] == 0) {
        si->ms_per_tick = 0.0;
        return;
    }
    si->ms_per_tick = 1000.0 / u64_to_double((uint64_t)si->frequency[1] << 32 | si->frequency[0]);
}

uint32_t systemtimer(void)
{
    int64_t counted;

    if (!os_qpc(&counted))
        return 0;
    return (uint32_t)(int64_t)((double)counted * g_sysinfo.ms_per_tick);
}

int gettimeofday(int *tv, int *tz)
{
    if (tv) {
        uint32_t ft[2];
        uint64_t t, us, s;

        os_getsystemtimeasfiletime(ft);
        t = ((uint64_t)ft[1] << 32 | ft[0]) - EPOCH_OFFSET;
        us = t / 10;
        s = us / 1000000;
        tv[0] = (int)(uint32_t)s;
        tv[1] = (int)((uint32_t)us - (uint32_t)s * 1000000u);
    }
    if (tz) {
        if (g_tzset_done == 0) {
            os_tzset();
            g_tzset_done++;
        }
        tz[0] = *os_timezone() / 60;
        tz[1] = *os_daylight();
    }
    return 0;
}

/* The result is an int. The seconds are far too big (see EPOCH_OFFSET), so the sum is out of
 * the range of an int and the result is -2147483648. */
int testtimer(void)
{
    int tv[2];

    gettimeofday(tv, NULL);
    return (int)((double)tv[0] * 1000.0 + (double)tv[1] * 0.001);
}

void debug_resetlogfile(void)
{
    Debug *d = &g_debug;

    if (g_debugit) {
        strcpy(d->log_path, "dedlog.txt");
        d->log_file = os_fopen(d->log_path, "wb");
        if (d->log_file) {
            os_fprintf(d->log_file, "loggy log log\r\n");
            os_fclose(d->log_file);
        }
    }
}

/* The play time is in units of 0.1 s. The average has no value when there is no account. */
void server_writelogfile(void)
{
    uint32_t total = 0, average = 0;
    int i, n = g_numofaccounts;

    for (i = 0; i < n; i++)
        total += (uint32_t)g_accounts[i].playtime / 10;
    if (n > 0)
        average = total / (uint32_t)n;
    g_slogfp = os_fopen("stats.txt", "w");
    if (g_slogfp) {
        os_fprintf(g_slogfp, "numofmissions=%d\r\n", sr_stats.numofmissions);
        os_fprintf(g_slogfp, "numofbullets=%d\r\n", sr_stats.numofbullets);
        os_fprintf(g_slogfp, "users=%d\r\n", g_numofaccounts);
        os_fprintf(g_slogfp, "totaltime=%d\r\n", total);
        os_fprintf(g_slogfp, "averagetime=%d\r\n", average);
        os_fprintf(g_slogfp, "\r\n");
        for (i = 0; i < g_numofaccounts; i++)
            os_fprintf(g_slogfp, "%d:%s:%d\r\n", i, g_accounts[i].name,
                       (uint32_t)g_accounts[i].playtime / 10);
        os_fclose(g_slogfp);
    }
}

uint64_t __cdecl __aulldiv(uint32_t low, uint32_t high, uint32_t divisor_low, uint32_t divisor_high)
{
    return ((uint64_t)high << 32 | low) / ((uint64_t)divisor_high << 32 | divisor_low);
}
