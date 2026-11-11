/* The view cone of the camera: a box test that tells if a box can be seen.
 *
 * The cone has five planes. Their normals are in camera space, so each test turns a normal
 * into world space with the camera's rotation. The normals are written to a table each time
 * the test runs.
 */
#include "render/frustum.h"

#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_camera cl_data_audio_camera
#define g_planes cl_data_frustum_planes

enum { NUM_PLANES = 5, NUM_CORNERS = 8 };

/* Write the five plane normals: straight ahead, left, right, up and down. */
static void make_planes(void)
{
    static const float directions[NUM_PLANES][3] = {
        {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}, {0.0f, 0.75f, 1.0f}, {0.0f, -0.75f, 1.0f}};
    int plane;

    for (plane = 0; plane < NUM_PLANES; ++plane) {
        g_planes[plane] = *(const vec3 *)directions[plane];
        normalizevector(&g_planes[plane], &g_planes[plane]);
    }
}

/* Tell if a box is at least partly in the view cone. The box goes from its smallest corner
 * to its largest corner. The result is 1 when no plane has all eight corners on its far side,
 * and 0 otherwise. */
int __cdecl boxinsidefrustum(const vec3 *smallest, const vec3 *largest)
{
    vec3 corner;
    vec3 normal;
    int plane;
    int index;

    make_planes();
    for (plane = 0; plane < NUM_PLANES; ++plane) {
        for (index = 0; index < NUM_CORNERS; ++index) {
            corner.x = index < 4 ? smallest->x : largest->x;
            corner.y = (index & 3) < 2 ? smallest->y : largest->y;
            corner.z = ((index & 3) == 0 || (index & 3) == 3) ? smallest->z : largest->z;
            subtractvectors(&corner, &corner, &g_camera.pos);
            projectvector(&normal, &g_planes[plane], g_camera.rot);
            if (dotproduct(&corner, &normal) < 0.0f)
                break;
        }
        if (index == NUM_CORNERS)
            return 0;
    }
    return 1;
}
