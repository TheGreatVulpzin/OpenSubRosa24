/* The unit vector of a packet. See unit_vector.c. */
#ifndef CL_MATH_UNIT_VECTOR_H
#define CL_MATH_UNIT_VECTOR_H

#include "sr_types.h"

void __cdecl unpackunitvector(const uint32_t *packed, float *out, uint8_t bits);

#endif /* CL_MATH_UNIT_VECTOR_H */
