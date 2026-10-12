/* Funções do servidor reescritas em C.
 * Cada função tem a mesma assinatura e convenção de chamada da original no 024d.
 */
#ifndef SR_SERVER_H
#define SR_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* events.c */
void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra);

#ifdef __cplusplus
}
#endif

#endif /* SR_SERVER_H */
