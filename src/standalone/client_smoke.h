/* Automatic input and frame checks for the standalone client. */
#ifndef CLIENT_SMOKE_H
#define CLIENT_SMOKE_H
enum ClientSmokeCall {
    SMOKE_CALL_SOUND = 0x4f450, SMOKE_CALL_SOUND_OBJECT = 0x4f150,
    SMOKE_CALL_GAME = 0x7b100, SMOKE_CALL_BUILDING = 0x58d20,
    SMOKE_CALL_LEVEL = 0x9c5c0, SMOKE_CALL_MODEL = 0xc14a0
};
int client_smoke_start(void);
void *client_smoke_import(const char *dll, const char *name, void *function);
void client_smoke_call(unsigned rva);
int client_smoke_finish(void);
#endif
