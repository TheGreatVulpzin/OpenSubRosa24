/* The entries of Debug.counters that the code uses. Page 2 of the diagnostic display shows all 16
 * entries as integers. */
#ifndef SR_DEBUG_COUNTERS_H
#define SR_DEBUG_COUNTERS_H

enum DebugCounter {
    DEBUG_COUNTER_SHOWN_TEAM = 4,        /* the team of player 0 */
    DEBUG_COUNTER_AFFINITY_SET = 5,      /* 1234 when SetProcessAffinityMask works */
    DEBUG_COUNTER_AFFINITY_BEFORE = 6,   /* the process mask plus 10000 */
    DEBUG_COUNTER_AFFINITY_AFTER = 7,    /* the process mask plus 10000 after the change */
    DEBUG_COUNTER_SHOWN_BONDS = 8,       /* the number of bonds */
    DEBUG_COUNTER_SHOWN_HUMANS = 9,      /* the active humans */
    DEBUG_COUNTER_SHOWN_PARTS = 10,      /* the joints and muscles of human 0 */
    DEBUG_COUNTER_SHOWN_MOUSE = 12       /* the left mouse button plus 100 */
};

#endif
