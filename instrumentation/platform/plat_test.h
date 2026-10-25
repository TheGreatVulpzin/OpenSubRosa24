/* Test environment of the offline harness (plat_test.c).
 *
 * A test runs one recorded call twice: run 0 is the lifted version, run 1 is the original.
 * plat_test_begin() gives each run the same start state. Each run writes a log of what it
 * does with the environment (packets, file writes, heap blocks). plat_test_same() compares
 * the two logs.
 */
#ifndef PLAT_TEST_H
#define PLAT_TEST_H

#include <stddef.h>

/* game_dir: the server folder (read-only). sandbox_dir: the folder for file writes. */
int plat_test_init(const char *game_dir, const char *sandbox_dir);

/* Install the stubs into the fread and fwrite pointers of the game (getpaths copies the
 * import addresses into two globals). Call after each snapshot load. */
void plat_test_patch_globals(void);

/* A packet that recv or recvfrom gives to the game (from the E lines of a recording). */
typedef struct PlatPacket {
    int ret;                  /* the return value: the byte count, or -1 */
    int len;                  /* bytes in data */
    unsigned char *data;
    int fromlen;              /* bytes in from (recvfrom) */
    unsigned char from[16];
} PlatPacket;

/* The packets of the next test call, in order. Each run gets the same packets. After the
 * last one, recv and recvfrom return -1 (no data). */
void plat_test_set_packets(const PlatPacket *packets, int count);

void plat_test_begin(int run);
void plat_test_end(int run);

/* 1 when run 0 and run 1 did the same. Otherwise 0, and why holds the first difference. */
int plat_test_same(char *why, size_t n);

/* Print the logs of both runs (HARNESS_DEBUG=2). */
void plat_test_dump(void);

/* Number of environment events in the last run (0 for a function that used no stub). */
int plat_test_events(void);

#endif /* PLAT_TEST_H */
