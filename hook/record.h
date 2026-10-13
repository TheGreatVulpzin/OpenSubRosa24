/* Record real inputs for the offline differential test. See record.c. */
#ifndef SV_RECORD_H
#define SV_RECORD_H

#include <stdint.h>

/* Called by the thunk of each lifted hook, with its arguments on the stack. */
void sv_record_args(uint32_t rva, int index, const uint32_t *args, int ndw);

/* Write the .data snapshot once, snapshot_after seconds after start. */
void sv_snapshot_check(void);

/* Record and replay of the side-effect functions (docs/side-effects.md).
 * The record-only thunks (hook/hooks_record.c) call these three around the original. */
void sv_rec_begin(uint32_t rva, const uint32_t *args, int ndw);
void sv_rec_push_ret(uint32_t ret);
uint32_t sv_rec_end(uint32_t eax);

/* True when a recorded call is open: the import wrappers then write events. */
int sv_rec_active(void);

/* One event line: "E <name>" + fields + "\n", added to every open recorded call.
 * Field helpers: a dword (V:), a string (s:hex), bytes that the import reads (b:hex),
 * bytes that the import writes (o:hex), the return value (r:). */
void sv_ev_begin(const char *name);
void sv_ev_v(uint32_t v);
void sv_ev_s(const char *s);
void sv_ev_bytes(char tag, const void *p, size_t n);
void sv_ev_hash(char tag, const void *p, size_t n);  /* " o#len:hash", not the bytes */
void sv_ev_r(uint32_t r);
void sv_ev_end(void);

/* Install the import wrappers (hook/record_imports.c). */
int sv_record_imports_install(void);

#endif /* SV_RECORD_H */
