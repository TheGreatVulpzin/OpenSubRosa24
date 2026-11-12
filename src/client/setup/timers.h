/* The profiler timers of the client. See timers.c. */
#ifndef CL_SETUP_TIMERS_H
#define CL_SETUP_TIMERS_H

void __cdecl resettimer(int timer, const char *name);
void __cdecl starttimer(int timer);

/* The end of a timer. Defined with the game profiler. */
void stoptimer(int timer);

#endif /* CL_SETUP_TIMERS_H */
