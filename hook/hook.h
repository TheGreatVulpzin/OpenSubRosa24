/* Sistema de hooks do SubVulpes.
 *
 * Cada hook troca o início de uma função do 024d por um JMP para a versão em C.
 * O trampolim permite chamar a função original (para o modo de verificação).
 */
#ifndef SV_HOOK_H
#define SV_HOOK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SvHook {
    const char *name;           /* nome da função (igual ao ELF 0.24c) */
    uint32_t rva;               /* início da função no 024d */
    void *replacement;          /* função nova */
    uint8_t prologue_len;       /* bytes copiados para o trampolim (>= 5, instruções inteiras) */
    const uint8_t *expect;      /* bytes originais esperados no início da função */
    const uint8_t *mask;        /* 0xff = conferir o byte; 0x00 = ignorar (endereço relocado) */
    void **original;            /* saída: ponteiro para chamar a original via trampolim */
    int enabled;                /* lido do subvulpes.ini */
    int installed;
} SvHook;

/* Log em subvulpes.log, na pasta do executável. */
void sv_log(const char *fmt, ...);

/* Lê subvulpes.ini. Devolve o valor da chave, ou def se a chave não existe. */
int sv_config_int(const char *key, int def);

/* Instala todos os hooks da tabela. Devolve o número de hooks instalados. */
int sv_install_hooks(SvHook *hooks, int count);

#ifdef __cplusplus
}
#endif

#endif /* SV_HOOK_H */
