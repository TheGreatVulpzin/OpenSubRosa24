/* Curves and surfaces of the temporary mesh. */
#include "mesh.h"

#include <string.h>

#include "sr_math.h"
#include "sr_runtime.h"
#include "sr_types.h"

#define g_tempmesh (*SR_AT(TempMesh, 0x256ad760))
#define g_bezier_curve SR_AT(float, 0x761ea20)
#define g_bezier_curve_uv SR_AT(float, 0x13c22d40)
#define g_bezier_curve_dv SR_AT(float, 0xb0661c0)

float tesselatebezier(int degree, float t, float a, float b, float c, float d)
{
    double u = (float)(1.0 - t);
    double v = t;
    switch (degree) {
    case 1:
        return (float)(u * a + d * v);
    case 2:
        return (float)(u * a * u + v * (u * ((double)b + b)) + v * d * v);
    case 3:
        return (float)(u * (c * 3.0) * v * v
            + (u * a * u * u + v * (u * (u * (b * 3.0)))) + v * d * v * v);
    default:
        /* Other degrees return the complement of t. */
        return (float)u;
    }
}

float tesselatebezierderivative(int degree, float t, float a, float b, float c, float d)
{
    double v = t;
    float result;
    switch (degree) {
    case 1:
        return (float)((double)d - a);
    case 2:
        return (float)((v * 2.0 * ((a - b * 2.0) + d) - a * 2.0) + b * 2.0);
    case 3:
        return (float)(b * 3.0 + (((v + v) * ((a * 3.0 - b * 6.0) + c * 3.0)
            + (((b * 3.0 - a) - c * 3.0) + d) * (v * (v * 3.0))) - a * 3.0));
    default:
        /* For another degree, the result is the bit pattern of degree. */
        memcpy(&result, &degree, sizeof(result));
        return result;
    }
}

static void bezier_surface(const float *points, const float *uvs, int steps_u, int degree_u,
                           int steps_v, int degree_v, int flags, int world_uv)
{
    int first = g_tempmesh.num_verts;
    float du = (float)(1.0 / (float)steps_u);
    float dv = (float)(1.0 / (float)steps_v);
    float v = 0.0f;
    int row, col, k, c;

    for (row = 0; row <= steps_v; row++) {
        float u = 0.0f;
        for (k = 0; k < 4; k++) {
            for (c = 0; c < 3; c++)
                g_bezier_curve[k * 3 + c] = tesselatebezier(degree_v, v,
                    points[k * 3 + c], points[12 + k * 3 + c],
                    points[24 + k * 3 + c], points[36 + k * 3 + c]);
            if (!world_uv) {
                for (c = 0; c < 2; c++)
                    g_bezier_curve_uv[k * 2 + c] = tesselatebezier(degree_v, v,
                        uvs[k * 2 + c], uvs[8 + k * 2 + c],
                        uvs[16 + k * 2 + c], uvs[24 + k * 2 + c]);
            }
            for (c = 0; c < 3; c++)
                g_bezier_curve_dv[k * 3 + c] = tesselatebezierderivative(degree_v, v,
                    points[k * 3 + c], points[12 + k * 3 + c],
                    points[24 + k * 3 + c], points[36 + k * 3 + c]);
        }
        for (col = 0; col <= steps_u; col++) {
            int n = g_tempmesh.num_verts;
            float g[6], x, y, z, length, inv;
            float *vertex = (float *)&g_tempmesh.verts[n];
            for (c = 0; c < 3; c++)
                vertex[c] = tesselatebezier(degree_u, u, g_bezier_curve[c],
                    g_bezier_curve[3 + c], g_bezier_curve[6 + c], g_bezier_curve[9 + c]);
            if (world_uv) {
                g_tempmesh.uvs[n].x = vertex[0];
                g_tempmesh.uvs[n].y = vertex[2];
            } else {
                float *uv = (float *)&g_tempmesh.uvs[n];
                for (c = 0; c < 2; c++)
                    uv[c] = tesselatebezier(degree_u, u, g_bezier_curve_uv[c],
                        g_bezier_curve_uv[2 + c], g_bezier_curve_uv[4 + c],
                        g_bezier_curve_uv[6 + c]);
            }
            for (c = 0; c < 3; c++)
                g[c] = tesselatebezierderivative(degree_u, u, g_bezier_curve[c],
                    g_bezier_curve[3 + c], g_bezier_curve[6 + c], g_bezier_curve[9 + c]);
            for (c = 0; c < 3; c++)
                g[3 + c] = tesselatebezier(degree_u, u, g_bezier_curve_dv[c],
                    g_bezier_curve_dv[3 + c], g_bezier_curve_dv[6 + c], g_bezier_curve_dv[9 + c]);
            x = (float)(g[4] * g[2] - g[5] * g[1]);
            y = (float)(g[0] * g[5] - g[3] * g[2]);
            z = (float)(g[1] * g[3] - g[4] * g[0]);
            g_tempmesh.normals[n].x = x;
            g_tempmesh.normals[n].y = y;
            g_tempmesh.normals[n].z = z;
            length = (float)x87_sqrt((float)(z * z + (x * x + y * y)));
            if (length == 0.0f) {
                x = y = z = 0.0f;
            } else {
                inv = (float)(1.0 / length);
                x = (float)(inv * x);
                y = (float)(y * inv);
                z = (float)(inv * z);
            }
            g_tempmesh.normals[n].x = x;
            g_tempmesh.normals[n].y = y;
            g_tempmesh.normals[n].z = z;
            length = (float)x87_sqrt((float)(g[2] * g[2] + (g[0] * g[0] + g[1] * g[1])));
            if (length == 0.0f) {
                x = y = z = 0.0f;
            } else {
                inv = (float)(1.0 / length);
                x = (float)(inv * g[0]);
                y = (float)(g[1] * inv);
                z = (float)(inv * g[2]);
            }
            g_tempmesh.tangents[n].x = x;
            g_tempmesh.tangents[n].y = y;
            g_tempmesh.tangents[n].z = z;
            g_tempmesh.num_verts++;
            u = (float)(u + du);
        }
        v = (float)(v + dv);
    }
    for (row = 0; row < steps_v; row++) {
        for (col = 0; col < steps_u; col++) {
            int b = (steps_u + 1) * row + first + col;
            int q = g_tempmesh.num_quads;
            g_tempmesh.quads[q].verts[0] = b;
            g_tempmesh.quads[q].verts[1] = b + 1;
            g_tempmesh.quads[q].verts[2] = b + steps_u + 2;
            g_tempmesh.quads[q].verts[3] = b + steps_u + 1;
            g_tempmesh.quad_slots[q] = flags;
            g_tempmesh.num_quads++;
        }
    }
}

void addbeziersurface(const float *points, const float *uvs, int steps_u, int degree_u,
                     int steps_v, int degree_v, int flags)
{
    bezier_surface(points, uvs, steps_u, degree_u, steps_v, degree_v, flags, 0);
}

void addbeziersurfacetexture(const float *points, const float *uvs, int steps_u, int degree_u,
                            int steps_v, int degree_v, int flags)
{
    bezier_surface(points, uvs, steps_u, degree_u, steps_v, degree_v, flags, 1);
}
