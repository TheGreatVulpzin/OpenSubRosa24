/* The object types (cars, helicopter, train): the lists of particles, bonds, faces and wheels
 * that createobject copies into an object. See object_types.c and vehicle_types.c. */
#ifndef SR_OBJECT_TYPES_H
#define SR_OBJECT_TYPES_H

#include "sr_runtime.h"
#include "sr_types.h"

#define sr_objecttypes sr_data_objecttype

void objecttype_addparticle(int type, float x, float y, float z, float mass);
int objecttype_addbond(int type, int subtype, int particle_a, int particle_b);
void objecttype_addface(int type, int corners, int v0, int v1, int v2, int v3, float extra, float armor);
void objecttype_addwheel(int type, int popped, int particle_a, int particle_b, const float *weights, float radius,
                         float mass, float inv_inertia, float ride_height, float spring, float damper,
                         float vertical);
void objecttype_calcquadweights(float weights[4], const vec3 *q, const vec3 p[4]);
void setupobjecttypeweight(int type);
void calculatepointscale(float *out, int n, const vec3 *points, const vec3 *light);

void setupobjecttypes_cars(void);
void setupobjecttypes_helicopters(void);
void setupobjectypes_train(void);

#endif
