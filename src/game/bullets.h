/* The bullets. See bullets.c. */
#ifndef SR_BULLETS_H
#define SR_BULLETS_H

#include "sr_runtime.h"
#include "sr_types.h"

/* A bullet type: real cartridge data in kilograms and meters. ItemType.bullet_type selects
 * one. */
typedef struct BulletType {
    float mass;    /* the recoil uses mass * bullet_speed / (gun mass + mass) */
    float drag;    /* the cross section over the mass: pi * caliber^2 / 4 / mass */
    float caliber; /* the diameter of the bullet */
    float length;  /* the length of the case */
    float unk_10;
    float unk_14;
} BulletType;

/* The four bullet types (setupbullettypes). */
#define g_bullettypes SR_AT(BulletType, 0xf437320)

void setupbullettypes(void);
int createbullet(int type, const vec3 *pos, const vec3 *vel, int owner);
void bullettimetolive(void);

#endif /* SR_BULLETS_H */
