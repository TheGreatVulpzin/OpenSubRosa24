/* Table of the lifted functions, for the offline differential test (src/tests/harness.c).
 * tools/lift/lift.py generates lifted_table.c. */
#ifndef LIFTED_TABLE_H
#define LIFTED_TABLE_H

#include <stdint.h>

enum { LF_RET_VOID, LF_RET_INT, LF_RET_INT64, LF_RET_FLOAT };

typedef struct LfEntry {
    const char *name;
    uint32_t rva;
    void *lifted;     /* lf_<rva> in lifted_024d.c */
    int dwords;       /* size of the arguments on the stack, in dwords */
    int ret;          /* LF_RET_* */
} LfEntry;

extern const LfEntry lf_table[];
extern const int lf_table_count;

#endif /* LIFTED_TABLE_H */
