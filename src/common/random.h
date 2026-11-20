/* The random numbers of the game. See random.c. */
#ifndef SR_RANDOM_H
#define SR_RANDOM_H

#include <stdint.h>

void init_genrand(uint32_t seed);
void init_by_array(const uint32_t *key, int length);
uint32_t genrand_int32(void);
uint32_t genrand_int31(void);
double genrand_real1(void);
double genrand_real2(void);
double genrand_real3(void);
double genrand_res53(void);
int getrandomint(void);
float getrandomfloat(void);
void getrandomvector2d(float out[2], float range, float min);
void getrandomvector3d(float out[3], float range, float min);
float serpnoise2d(float x, float y, uint32_t mask);
float lerpnoise2d(float x, float y, uint32_t mask);
float cerpnoise2d(float x, float y, uint32_t mask);

#endif /* SR_RANDOM_H */
