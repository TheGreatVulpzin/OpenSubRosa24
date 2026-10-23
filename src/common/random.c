/* The random numbers of the game.
 *
 * genrand_int32 is the Mersenne Twister MT19937, with its state in two game globals.
 * The other functions use an integer hash: the random vectors hash a global counter, and
 * serpnoise2d hashes the corners of a grid cell (value noise).
 */
#include "random.h"

#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals: the state of the Mersenne Twister and the counter of the random vectors. */
#define g_mt SR_AT(uint32_t, 0x60dc0) /* 624 words */
#define g_mti (*SR_AT(int, 0x60cd0))  /* 625: no seed yet */
#define g_random_counter (*SR_AT(uint32_t, 0x916d2c0))

#define MT_N 624
#define MT_M 397
#define MT_MATRIX 0x9908b0dfu
#define MT_UPPER 0x80000000u
#define MT_LOWER 0x7fffffffu

void init_genrand(uint32_t seed)
{
    int i;
    g_mt[0] = seed;
    for (i = 1; i < MT_N; i++)
        g_mt[i] = 1812433253u * (g_mt[i - 1] ^ (g_mt[i - 1] >> 30)) + (uint32_t)i;
    g_mti = MT_N;
}

static uint32_t mt_twist(uint32_t a, uint32_t b, uint32_t far)
{
    uint32_t y = (a & MT_UPPER) | (b & MT_LOWER);
    return far ^ (y >> 1) ^ (y & 1 ? MT_MATRIX : 0);
}

uint32_t genrand_int32(void)
{
    uint32_t y;
    int k;
    if (g_mti >= MT_N) {
        if (g_mti == MT_N + 1)
            init_genrand(5489);
        for (k = 0; k < MT_N - MT_M; k++)
            g_mt[k] = mt_twist(g_mt[k], g_mt[k + 1], g_mt[k + MT_M]);
        for (; k < MT_N - 1; k++)
            g_mt[k] = mt_twist(g_mt[k], g_mt[k + 1], g_mt[k + MT_M - MT_N]);
        g_mt[MT_N - 1] = mt_twist(g_mt[MT_N - 1], g_mt[0], g_mt[MT_M - 1]);
        g_mti = 0;
    }
    y = g_mt[g_mti++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    return y ^ (y >> 18);
}

/* A value in (-1, 1] from an integer. */
static float hash_unit(uint32_t n)
{
    uint32_t v;
    n ^= n << 13;
    v = n * n * 15731u + 789221u;
    return (float)(1.0 - (double)((v * v + 0xd208dd0du) & 0x7fffffff) * 9.313225746154785E-10);
}

static float random_unit(void)
{
    return hash_unit(g_random_counter++);
}

/* A random direction in x and y, with a length from min to min + range. */
void getrandomvector2d(float out[2], float range, float min)
{
    float length;
    out[0] = random_unit();
    out[1] = random_unit();
    normalizevector2d(out, out);
    length = (float)((random_unit() + 1.0) * range * 0.5 + min);
    out[0] = length * out[0];
    out[1] = (float)(length * out[1]);
}

/* A random direction in 3D, with a length from min to min + range. */
void getrandomvector3d(float out[3], float range, float min)
{
    float length;
    out[0] = random_unit();
    out[1] = random_unit();
    out[2] = random_unit();
    normalizevector((vec3 *)out, (const vec3 *)out);
    length = (float)((random_unit() + 1.0) * range * 0.5 + min);
    out[0] = length * out[0];
    out[1] = (float)(length * out[1]);
    out[2] = (float)(length * out[2]);
}

/* Linear interpolation of the four grid corners. mask wraps the next cell numbers. */
float lerpnoise2d(float x, float y, uint32_t mask)
{
    int x0 = (int)(long long)x, y0 = (int)(long long)y;
    uint32_t x1 = ((uint32_t)x0 + 1) & mask;
    uint32_t row0 = (uint32_t)y0 * 57, row1 = (((uint32_t)y0 + 1) & mask) * 57;
    float n00 = hash_unit(row0 + (uint32_t)x0), n10 = hash_unit(x1 + row0);
    float n01 = hash_unit(row1 + (uint32_t)x0), n11 = hash_unit(row1 + x1);
    float fx = (float)(x - (float)x0);
    return (float)((float)(y - (float)y0) * (double)(float)(n11 * (double)fx + n01 * (1.0 - fx))
        + (float)(n00 * (1.0 - fx) + n10 * (double)fx) * (1.0 - ((double)y - (float)y0)));
}

float cerpnoise2d(float x, float y, uint32_t mask)
{
    int x0 = (int)(long long)x, y0 = (int)(long long)y;
    uint32_t x1 = ((uint32_t)x0 + 1) & mask;
    uint32_t row0 = (uint32_t)y0 * 57, row1 = (((uint32_t)y0 + 1) & mask) * 57;
    float n00 = hash_unit(row0 + (uint32_t)x0), n10 = hash_unit(x1 + row0);
    float n01 = hash_unit(row1 + (uint32_t)x0), n11 = hash_unit(row1 + x1);
    float sx = (float)((1.0 - (float)x87_cos((float)(x - (float)x0) * 3.14159265359)) * 0.5);
    float sy = (float)((1.0 - (float)x87_cos((float)(y - (float)y0) * 3.14159265359)) * 0.5);
    return (float)(sy * (double)(float)(n11 * (double)sx + n01 * (1.0 - sx))
        + (float)(n00 * (1.0 - sx) + sx * (double)n10) * (1.0 - sy));
}

/* Smooth interpolation between the four grid corners. */
float serpnoise2d(float x, float y, uint32_t mask)
{
    int x0 = (int)(long long)x, y0 = (int)(long long)y;
    uint32_t x1 = ((uint32_t)x0 + 1) & mask;
    uint32_t row0 = (uint32_t)y0 * 57, row1 = (((uint32_t)y0 + 1) & mask) * 57;
    float n00 = hash_unit(row0 + (uint32_t)x0), n10 = hash_unit(x1 + row0);
    float n01 = hash_unit(row1 + (uint32_t)x0), n11 = hash_unit(row1 + x1);
    float fx = (float)(x - (float)x0), fy, sx, sy;
    sx = (float)((3.0 - fx * 2.0) * (fx * fx));
    fy = (float)(y - (float)y0);
    sy = (float)(fy * fy * (3.0 - fy * 2.0));
    return (float)((1.0 - sy) * (float)((1.0 - sx) * n00 + n10 * sx) + (float)(sx * n11 + n01 * (1.0 - sx)) * sy);
}

/* Seeds the generator from an array of numbers. The array is read again from the start when
 * it is shorter than the 624 words of the state. */
void init_by_array(const uint32_t *key, int length)
{
    uint32_t *mt = g_mt;
    int i = 1, j = 0, k;

    init_genrand(19650218u);
    k = MT_N > length ? MT_N : length;
    for (; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
        i++;
        j++;
        if (i >= MT_N) {
            mt[0] = mt[MT_N - 1];
            i = 1;
        }
        if (j >= length)
            j = 0;
    }
    for (k = MT_N - 1; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
        i++;
        if (i >= MT_N) {
            mt[0] = mt[MT_N - 1];
            i = 1;
        }
    }
    mt[0] = MT_UPPER;
}
