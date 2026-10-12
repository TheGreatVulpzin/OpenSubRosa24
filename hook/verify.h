/* Verificação genérica: roda a função nova e a original e compara a memória.
 *
 * Durante cada execução, a seção .data do jogo fica somente leitura. Cada escrita
 * gera uma exceção: o verificador guarda a página antes da escrita e libera a página.
 * No fim ele sabe quais páginas cada versão alterou e compara só essas.
 *
 * Fluxo de um hook (ver SV_VERIFY_CALL em hooks_024d.c):
 *   sv_verify_should(v)    decide se esta chamada é verificada (amostragem)
 *   sv_track_begin_new(v)            nova roda
 *   sv_track_switch_to_original()    roda original
 *   sv_verify_end(v, ...)  compara, registra no log e mantém o resultado da original
 *
 * Limites: só cobre a seção .data do executável (todas as globais do jogo).
 * Escritas na pilha, no heap ou em DLLs não entram. O servidor precisa ter uma só thread.
 */
#ifndef SV_VERIFY_H
#define SV_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SvVerify {
    const char *name;
    long every;      /* verifica 1 a cada N chamadas. Valor negativo = padrão do hook;
                        subvulpes.ini verify_every.<nome> tem prioridade. */
    long calls;      /* chamadas totais */
    long checks;     /* chamadas verificadas */
    long fails;      /* verificações com diferença */
    /* Escopo aprendido: faixas de páginas de .data que a função costuma escrever.
     * Fora das rodadas completas, só essas faixas ficam protegidas. */
    unsigned nranges;
    unsigned range[32][2];   /* [primeira página, última + 1) dentro de .data */
} SvVerify;

/* Só para testes: usa outra seção do executável no lugar de .data. Chame antes de sv_verify_init. */
void sv_verify_set_section(const char *name);

/* Localiza a seção .data e o rand() do MSVCR100. Devolve 0 se falhar. */
int sv_verify_init(void);

/* 1 = verificar esta chamada. Respeita verify=0 e a amostragem. */
int sv_verify_should(SvVerify *v);

/* Começa a registrar escritas. Chame antes da versão nova.
 * Nas primeiras verificações e a cada verify_full_every (padrão 50), protege a .data
 * inteira e aprende as globais escritas. Nas outras, protege só o escopo aprendido. */
void sv_track_begin_new(SvVerify *v);

/* Guarda o resultado da versão nova, desfaz as escritas e começa a registrar
 * de novo. Chame entre a versão nova e a original. */
void sv_track_switch_to_original(void);

/* Para de registrar, compara e mantém o resultado da original.
 * same_return: 1 se os valores de retorno são iguais (ou se a função é void).
 * detail: texto curto para o log (pode ser NULL). */
void sv_verify_end(SvVerify *v, int same_return, const char *detail);

/* Resumo de um hook no log. */
void sv_verify_summary(const SvVerify *v);

#ifdef __cplusplus
}
#endif

#endif /* SV_VERIFY_H */
