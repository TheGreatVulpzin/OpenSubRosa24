/* Hook wrappers: verification, sampling and crash protection.
 *
 * NEW and ORIG are the full calls, with arguments.
 * - Broken hook (the new version crashed before): only the original runs.
 * - Called while another verification runs (nested call): only the original runs.
 *   Both runs of the outer function then see the same callee behavior.
 * - No verification in this call: the new version runs. If it crashes, the original runs.
 * - With verification: both run and are compared. The game keeps the result of the original.
 *   If the new version crashes, the verifier undoes its writes before the original runs.
 */
#ifndef SV_HOOK_WRAP_H
#define SV_HOOK_WRAP_H

#include "verify.h"

#define SV_HOOK_VOID(STAT, NEW, ORIG, DETAIL)                                          \
    do {                                                                               \
        sv_note_call((STAT).name);                                                     \
        if ((STAT).broken || sv_verify_busy() || sv_original_only()) {                                       \
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

/* With a return value: compare the returns (converted to CMPTYPE) and return the one of the original. */
#define SV_HOOK_RET_CMP(TYPE, CMPTYPE, STAT, NEW, ORIG, DETAIL)                                     \
    do {                                                                               \
        TYPE r_new_ = 0, r_orig_;                                                      \
        sv_note_call((STAT).name);                                                     \
        if ((STAT).broken || sv_verify_busy() || sv_original_only())                                         \
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
        sv_verify_end(&(STAT), (CMPTYPE)r_new_ == (CMPTYPE)r_orig_, DETAIL);                             \
        return r_orig_;                                                                \
    } while (0)

/* Integer and pointer returns: exact comparison. */
#define SV_HOOK_RET(TYPE, STAT, NEW, ORIG, DETAIL) SV_HOOK_RET_CMP(TYPE, TYPE, STAT, NEW, ORIG, DETAIL)

/* x87 float returns: compare after rounding to float. The original often rounds to float
 * before it returns, and the Ghidra pseudo-C loses that rounding. Callers store the
 * result as float, so equal floats mean an equal effect. The game keeps the original value. */
#define SV_HOOK_RET_FLOAT(TYPE, STAT, NEW, ORIG, DETAIL) SV_HOOK_RET_CMP(TYPE, float, STAT, NEW, ORIG, DETAIL)

#endif /* SV_HOOK_WRAP_H */
