/* The packets of the game: a bit stream to write and to read. See packet.c. */
#ifndef SR_PACKET_H
#define SR_PACKET_H

#include <stdint.h>

#include "sr_types.h"

int pbitwrite(const uint32_t *value, uint32_t bits);
int pbitread(uint32_t *value, uint32_t bits);
int pbitwritenego(const uint32_t *value, uint32_t bits);
int pbitreadnego(uint32_t *value, uint32_t bits);
int cs_pwrite(const void *data, int size, int count);
int cs_pread(void *dst, int size, int count);
int recvpacket(void);
void sendpacket(uint32_t ip, uint16_t port);
void packangle(const float *angle, uint32_t bits);
void packposition(const vec3 *pos);
void packorientation(const vec3 *dir, uint32_t bits);
void unpackorientation(vec3 *out, uint32_t bits);
void packscalar(const float *x, float range, uint32_t bits);
float unpackscalar(uint32_t raw, uint32_t bits);
void unpackangle(float *out, uint32_t bits);
void packunitvector(uint32_t out[3], const float v[3], uint8_t bits);
void packpositiondelta(const vec3 *a, const vec3 *b, float range, uint32_t bits);
void unpackpositiondelta(vec3 *out, const vec3 *ref, float range, uint32_t bits);

#endif /* SR_PACKET_H */
