/* Copy and clear of small vectors. See small_vectors.c. */
#ifndef CL_MATH_SMALL_VECTORS_H
#define CL_MATH_SMALL_VECTORS_H

void __cdecl copyvector(unsigned int *to, const unsigned int *from);
void __cdecl copyvector2d(unsigned int *to, const unsigned int *from);
void __cdecl zerovector(unsigned int *vector);
float __cdecl flabs(float value);

#endif /* CL_MATH_SMALL_VECTORS_H */
