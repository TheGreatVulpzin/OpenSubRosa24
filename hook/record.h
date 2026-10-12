/* Record real inputs for the offline differential test. See record.c. */
#ifndef SV_RECORD_H
#define SV_RECORD_H

#include <stdint.h>

/* Called by the thunk of each lifted hook, with its arguments on the stack. */
void sv_record_args(uint32_t rva, int index, const uint32_t *args, int ndw);

/* Write the .data snapshot once, snapshot_after seconds after start. */
void sv_snapshot_check(void);

#endif /* SV_RECORD_H */
