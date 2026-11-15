/* The color of the sun light and of the sky. The air is a sphere of radius 512 around the
 * origin, and the eye is at height 496. Light that crosses the air loses red, green and blue
 * at different rates, so a low sun is red and the sky is blue. */
#include "render/sky_light.h"

#include "client.h"
#include "math/small_vectors.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* The loss of red, green and blue for each unit of air depth. */
static const float g_scatter[3] = {0.5f, 0.625f, 1.0f};

static const vec3 g_eye_start = {0.0f, 496.0f, 0.0f};

/* The five directions of the ambient light: up, plus and minus x, plus and minus z. */
static const vec3 g_up = {0.0f, 1.0f, 0.0f};
static const vec3 g_east = {1.0f, 0.0f, 0.0f};
static const vec3 g_west = {-1.0f, -0.0f, -0.0f};
static const vec3 g_south = {0.0f, 0.0f, 1.0f};
static const vec3 g_north = {-0.0f, -0.0f, -1.0f};

enum { AIR_RADIUS = 512 };

/* The distance from a point inside the sphere to its surface along a unit direction. */
float __cdecl raydistancesphere(const vec3 *from, const vec3 *dir, float radius)
{
    float along = dotproduct(from, dir);
    float rest = (float)(dotproduct(from, from) - (double)along * along);
    return (float)(x87_sqrt((double)radius * radius - rest) - along);
}

/* The share of red, green and blue that stays in a ray from one point to another. */
void __cdecl getsuncolor(float out[3], const vec3 *from, const vec3 *to)
{
    vec3 path;
    float length, depth;

    subtractvectors(&path, to, from);
    length = vectorlength(&path);
    depth = (float)((float)((float)(length * 0.75) / 512.0) * 20.0);
    out[0] = (float)x87_exp(-(double)depth * g_scatter[0]);
    out[1] = (float)x87_exp(-(double)depth * g_scatter[1]);
    out[2] = (float)x87_exp(-(double)depth * g_scatter[2]);
}

/* Add the light of the sky in one direction to the ambient color. The ray stops at 0.3125 of
 * the distance to the edge of the air. */
static void add_sky_light(vec3 *ambient, const vec3 *eye, const vec3 *dir, float weight)
{
    vec3 edge = *dir;
    float color[3];
    float distance = raydistancesphere(eye, &edge, (float)AIR_RADIUS);

    scaleaddvectors(&edge, eye, &edge, (float)(distance * 0.3125));
    getsuncolor(color, eye, &edge);
    scaleaddvectors(ambient, ambient, (const vec3 *)color, weight);
}

/* Set the diffuse color of the sun and the ambient color of the sky for the frame. */
void __cdecl getsuncolors(void)
{
    const vec3 *sun = (const vec3 *)cl_data_light_position;
    vec3 *diffuse = (vec3 *)cl_data_light_diffuse;
    vec3 *ambient = (vec3 *)cl_data_light_ambient;
    vec3 eye = g_eye_start;
    vec3 edge;
    float distance;
    double low;

    distance = raydistancesphere(&eye, sun, (float)AIR_RADIUS);
    scaleaddvectors(&edge, &eye, sun, distance);
    getsuncolor(&diffuse->x, &eye, &edge);
    scalevector(diffuse, diffuse, 1.5f);
    zerovector((unsigned int *)ambient);
    add_sky_light(ambient, &eye, &g_up, 0.75f);
    add_sky_light(ambient, &eye, &g_east, 0.5f);
    add_sky_light(ambient, &eye, &g_west, 0.5f);
    add_sky_light(ambient, &eye, &g_south, 0.5f);
    add_sky_light(ambient, &eye, &g_north, 0.5f);
    /* A low sun gives less sky light. The factor is 1 - (1 - y) to the power of 4. */
    low = 1.0 - sun->y;
    scalevector(ambient, ambient, (float)(1.0 - low * (low * (low * low))));
    scalevector(ambient, ambient, 0.75f);
    scalevector(diffuse, diffuse, 0.75f);
}

/* The sky color near the horizon, in the direction of the sun. Each part is 1 at most. */
void __cdecl getskycolor(float color[3])
{
    const vec3 *sun = (const vec3 *)cl_data_light_position;
    vec3 eye = g_eye_start;
    vec3 edge, path, middle, view;
    float distance, length, depth, phase, cos_angle;
    float light[3];
    double kept[3];
    int i;

    distance = raydistancesphere(&eye, sun, (float)AIR_RADIUS);
    scaleaddvectors(&edge, &eye, sun, distance);
    subtractvectors(&path, &edge, &eye);
    length = vectorlength(&path);
    scaleaddvectors(&middle, &eye, &path, 0.375f);
    /* The sun light that reaches the middle point of the path. */
    depth = (float)((float)(raydistancesphere(&middle, sun, (float)AIR_RADIUS) / 512.0) * 20.0);
    for (i = 0; i < 3; ++i)
        kept[i] = x87_exp(-(double)depth * g_scatter[i]);
    subtractvectors(&view, &edge, &eye);
    normalizevector(&view, &view);
    cos_angle = dotproduct(&view, sun);
    phase = (float)((double)cos_angle * cos_angle + 1.0);
    depth = (float)((float)(length / 512.0) * 20.0);
    for (i = 0; i < 3; ++i)
        light[i] = (float)(kept[i] * depth * phase * g_scatter[i]);
    /* The light that stays on the way from the middle point to the eye. */
    depth = (float)((float)((float)(length * 0.375) / 512.0) * 20.0);
    for (i = 0; i < 3; ++i)
        kept[i] = x87_exp(-(double)depth * g_scatter[i]);
    for (i = 0; i < 3; ++i)
        color[i] = (float)(light[i] * kept[i]);
    for (i = 0; i < 3; ++i)
        if (1.0 < color[i])
            color[i] = 1.0f;
}
