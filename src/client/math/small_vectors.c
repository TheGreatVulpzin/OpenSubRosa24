/* Copy and clear of small vectors, and the absolute value of a float. A copy moves the 32 bit
 * words as they are, so a value that is not a number keeps its bits. */
#include "math/small_vectors.h"

#include <math.h>

#include "client.h"

/* Copy the three words of a 3D vector. */
void __cdecl copyvector(unsigned int *to, const unsigned int *from)
{
    to[0] = from[0];
    to[1] = from[1];
    to[2] = from[2];
}

/* Copy the two words of a 2D vector. */
void __cdecl copyvector2d(unsigned int *to, const unsigned int *from)
{
    to[0] = from[0];
    to[1] = from[1];
}

/* Set the three words of a 3D vector to zero. */
void __cdecl zerovector(unsigned int *vector)
{
    vector[0] = 0;
    vector[1] = 0;
    vector[2] = 0;
}

/* The absolute value of a float. */
float __cdecl flabs(float value)
{
    return (float)fabs(value);
}
