/* The clock, the process and the log files of the game. See system.c. */
#ifndef SR_SYSTEM_H
#define SR_SYSTEM_H

#include <stdint.h>

/* Binds the process to the first CPU when it may run there. The server never calls it. */
void setcpuaffinity(void);

/* Reads the frequency of the performance counter and the length of one tick in milliseconds. */
void getcpufrequency(void);

/* The performance counter in milliseconds, cut to 32 bits. 0 when the counter fails. */
uint32_t systemtimer(void);

/* The time of day as seconds and microseconds in tv, and the time zone in tz. */
int gettimeofday(int *tv, int *tz);

/* The time of gettimeofday in milliseconds, as an int. */
int testtimer(void);

/* Writes dedlog.txt when the debug flag is on. */
void debug_resetlogfile(void);

/* Writes stats.txt: the totals and the play time of each account. */
void server_writelogfile(void);

/* The 64-bit unsigned divide of the compiler, with the arguments as four 32-bit words. */
uint64_t __cdecl __aulldiv(uint32_t low, uint32_t high, uint32_t divisor_low, uint32_t divisor_high);

#endif /* SR_SYSTEM_H */
