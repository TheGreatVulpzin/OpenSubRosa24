/* Hook wrappers: verification, sampling and crash protection.
 *
 * NEW and ORIG are the full calls, with arguments.
 * - Broken hook (the new version crashed before): only the original runs.
 * - No verification in this call: the new version runs. If it crashes, the original runs.
 * - With verification: both run and are compared. The game keeps the result of the original.
 *   If the new version crashes, the verifier undoes its writes before the original runs.
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

/* With a return value: compare the returns and return the one of the original. */
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
