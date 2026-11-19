/* The model of a bundle of cash: a box of the given size around the origin, with fixed texture
 * coordinates on the two triangles of the first face and the last face.
 */
#include "textures/cash_model.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/models.h"

#define g_temp cl_data_modeltemp

/* The triangles with fixed texture coordinates: 0 and 1, and 10 and 11. */
#define CASH_TRI_FIRST 0
#define CASH_TRI_LAST 10

/* Corner coordinates of the two triangles of one face: u and v of each corner. */
static const float cash_side[2][6] = {
    {0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f},
    {0.0f, 0.0f, 1.0f, 0.5f, 1.0f, 0.0f},
};
static const float cash_end[2][6] = {
    {0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.5f},
};

/* Copy the six texture values of one triangle. */
static void set_corners(TriTexCoords *tri, const float *values)
{
    int corner;

    for (corner = 0; corner < 3; corner++) {
        tri->corners[corner].u = values[corner * 2];
        tri->corners[corner].v = values[corner * 2 + 1];
    }
}

/* Build the model of cash. The box has half sizes a, b and c. Every triangle gets zero texture
 * coordinates before the fixed ones are set. */
void __cdecl model_createcash(int model, float a, float b, float c)
{
    static const vec3 origin = {0.0f, 0.0f, 0.0f};
    int tri;

    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    addboxtomodel(&origin, a, b, c);
    for (tri = 0; tri < g_temp.num_tris; tri++) {
        TriTexCoords *t = &g_temp.tri_texcoords[tri];
        int corner;
        for (corner = 0; corner < 3; corner++)
            t->corners[corner].u = t->corners[corner].v = 0.0f;
    }
    set_corners(&g_temp.tri_texcoords[CASH_TRI_LAST], cash_end[0]);
    set_corners(&g_temp.tri_texcoords[CASH_TRI_LAST + 1], cash_end[1]);
    set_corners(&g_temp.tri_texcoords[CASH_TRI_FIRST], cash_side[0]);
    set_corners(&g_temp.tri_texcoords[CASH_TRI_FIRST + 1], cash_side[1]);
    setupmodel(model, 0);
}
