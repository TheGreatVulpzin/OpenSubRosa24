/* Automatic input and frame checks for the standalone client. */
#ifndef CLIENT_SMOKE_H
#define CLIENT_SMOKE_H
int client_smoke_start(void);
void *client_smoke_import(const char *dll, const char *name, void *function);
void client_smoke_call(unsigned rva);
int client_smoke_finish(void);
#endif
