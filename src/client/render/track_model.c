/* The buffer of the track.
 *
 * For each straight piece of the track, the sleepers under the two rails. A piece has 7 pairs
 * of rail triangles. Each pair gives one sleeper end on each side of the track: a block of 8
 * corners, drawn as 3 quads (the inner side, the top and the outer side). rendertrackmodel draws
 * the buffer.
 */
#include "render/track_model.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_track sr_data_track
#define g_vbo cl_data_vbo
#define glBindBufferARB ((GlBindBufferFn)cl_data_gl_bind_buffer)
#define glBufferDataARB ((GlBufferDataFn)cl_data_gl_buffer_data)

#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88e4
#define TRACK_STRAIGHT 0
#define RAIL_TRIS 14
#define SLEEPER_HALF 0.03125f /* half the width of a sleeper end */
#define SLEEPER_REACH 0.3175f /* the distance of a sleeper end from the rail line */
#define SLEEPER_HEIGHT 0.125f
#define SLEEPER_U 0.5f
#define SLEEPER_FACES 3

/* Game functions of other files. */
#define negvector CL_FN_NAMED(negvector, 0x50b980, void(__cdecl *)(vec3 *out, const vec3 *v))

/* The 8 corners of a sleeper end: corners 0 and 3 at the first rail point, 4 and 7 at the
 * second, each pair the width of the sleeper apart, moved by reach along the first rail.
 * Corners 1, 2, 5 and 6 are above them. */
static void sleeper_corners(vec3 corners[8], const vec3 *first, const vec3 *second, const vec3 *dir_a,
                            const vec3 *dir_b, float reach)
{
    scaleaddvectors(&corners[0], first, dir_a, -SLEEPER_HALF);
    scaleaddvectors(&corners[0], &corners[0], dir_a, reach);
    scaleaddvectors(&corners[1], &corners[0], &sr_data_yaxis, SLEEPER_HEIGHT);
    scaleaddvectors(&corners[3], first, dir_a, SLEEPER_HALF);
    scaleaddvectors(&corners[3], &corners[3], dir_a, reach);
    scaleaddvectors(&corners[2], &corners[3], &sr_data_yaxis, SLEEPER_HEIGHT);
    scaleaddvectors(&corners[4], second, dir_b, -SLEEPER_HALF);
    scaleaddvectors(&corners[4], &corners[4], dir_a, reach);
    scaleaddvectors(&corners[5], &corners[4], &sr_data_yaxis, SLEEPER_HEIGHT);
    scaleaddvectors(&corners[7], second, dir_b, SLEEPER_HALF);
    scaleaddvectors(&corners[7], &corners[7], dir_a, reach);
    scaleaddvectors(&corners[6], &corners[7], &sr_data_yaxis, SLEEPER_HEIGHT);
}

/* Add the 3 quads of a sleeper end to the buffer. The normals are the back of the rail
 * direction, up and the rail direction. */
static void sleeper_quads(const vec3 corners[8], const vec3 *dir_a)
{
    int face, k;

    for (face = 0; face < SLEEPER_FACES; face++) {
        vec3 normal;

        if (face == 0)
            negvector(&normal, dir_a);
        else if (face == 1)
            normal = sr_data_yaxis;
        else
            normal = *dir_a;
        for (k = 0; k < 4; k++) {
            ModelVertex *out = &cl_data_track_verts[cl_data_track_vertex_count];

            out->pos = corners[sr_data_tracktrailmodelquad[face * 4 + k]];
            out->texcoord.u = SLEEPER_U;
            out->texcoord.v = 0.0f;
            out->normal = normal;
            cl_data_track_vertex_count++;
        }
    }
}

/* Make the buffer of the track and send it to OpenGL. */
void __cdecl buildtrackmodel(void)
{
    vec3 corners[8], dir_a, dir_b;
    int piece, pair;

    cl_data_track_vertex_floats = (int)(sizeof(ModelVertex) / sizeof(float));
    cl_data_track_vertex_count = 0;
    for (piece = 0; piece < sr_data_numoftracks; piece++) {
        const TrackPiece *track = &g_track[piece];

        if (track->type != TRACK_STRAIGHT)
            continue;
        for (pair = 0; pair < RAIL_TRIS; pair += 2) {
            const int *a = track->tris[pair], *b = track->tris[pair + 1];

            subtractvectors(&dir_a, &track->verts[a[1]], &track->verts[a[0]]);
            normalizevector(&dir_a, &dir_a);
            subtractvectors(&dir_b, &track->verts[b[1]], &track->verts[b[2]]);
            normalizevector(&dir_b, &dir_b);
            sleeper_corners(corners, &track->verts[a[0]], &track->verts[b[2]], &dir_a, &dir_b, SLEEPER_REACH);
            sleeper_quads(corners, &dir_a);
            sleeper_corners(corners, &track->verts[a[1]], &track->verts[b[1]], &dir_a, &dir_b, -SLEEPER_REACH);
            sleeper_quads(corners, &dir_a);
        }
    }
    glBindBufferARB(GL_ARRAY_BUFFER, g_vbo[cl_data_track_vbo].gl_name);
    glBufferDataARB(GL_ARRAY_BUFFER, cl_data_track_vertex_count * cl_data_track_vertex_floats * 4, cl_data_track_verts,
                    GL_STATIC_DRAW);
    glBindBufferARB(GL_ARRAY_BUFFER, 0);
}
