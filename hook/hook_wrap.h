/* Wrappers dos hooks: verificação, amostragem e proteção contra crash.
 *
 * NEW e ORIG são as chamadas completas, com argumentos.
 * - Hook quebrado (a versão nova já travou): só a original roda.
 * - Sem verificação nesta chamada: roda a nova. Se ela travar, roda a original.
 * - Com verificação: roda as duas e compara. O jogo fica com o resultado da original.
 *   Se a nova travar, o verificador desfaz as escritas dela antes de rodar a original.
 */
#ifndef SV_HOOK_WRAP_H
#define SV_HOOK_WRAP_H

#include "verify.h"

#define SV_HOOK_VOID(STAT, NEW, ORIG, DETAIL)                                          \
    do {                                                                               \
        if ((STAT).broken) {                                                           \
            ORIG;                                                                      \
            return;                                                                    \
        }                                                                              \
        if (!sv_verify_should(&(STAT))) {                                              \
            __try {                                                                    \
                NEW;                                                                   \
            } __except (sv_crash_filter(&(STAT), GetExceptionInformation())) {         \
                ORIG;                                                                  \
            }                                                                          \
            return;                                                                    \
        }                                                                              \
        sv_track_begin_new(&(STAT));                                                   \
        __try {                                                                        \
            NEW;                                                                       \
        } __except (sv_crash_filter(&(STAT), GetExceptionInformation())) {             \
        }                                                                              \
        sv_track_switch_to_original();                                                 \
        ORIG;                                                                          \
        sv_verify_end(&(STAT), 1, DETAIL);                                             \
    } while (0)

/* Com retorno: compara os retornos e devolve o da original. */
#define SV_HOOK_RET(TYPE, STAT, NEW, ORIG, DETAIL)                                     \
    do {                                                                               \
        TYPE r_new_ = 0, r_orig_;                                                      \
        if ((STAT).broken)                                                             \
            return ORIG;                                                               \
        if (!sv_verify_should(&(STAT))) {                                              \
            __try {                                                                    \
                r_new_ = NEW;                                                          \
            } __except (sv_crash_filter(&(STAT), GetExceptionInformation())) {         \
                r_new_ = ORIG;                                                         \
            }                                                                          \
            return r_new_;                                                             \
        }                                                                              \
        sv_track_begin_new(&(STAT));                                                   \
        __try {                                                                        \
            r_new_ = NEW;                                                              \
        } __except (sv_crash_filter(&(STAT), GetExceptionInformation())) {             \
        }                                                                              \
        sv_track_switch_to_original();                                                 \
        r_orig_ = ORIG;                                                                \
        sv_verify_end(&(STAT), r_new_ == r_orig_, DETAIL);                             \
        return r_orig_;                                                                \
    } while (0)

#endif /* SV_HOOK_WRAP_H */
