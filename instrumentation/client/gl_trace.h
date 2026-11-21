/* Record graphics calls for a crash report when SR_TRACE=4. */
#ifndef CLIENT_GL_TRACE_H
#define CLIENT_GL_TRACE_H
void *client_gl_trace_import(const char *dll, const char *name, void *function);
void client_gl_trace_slots(void);
void client_gl_trace_dump(void);
#endif
