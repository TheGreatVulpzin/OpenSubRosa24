/* Curves and surfaces of the temporary mesh. */
#ifndef SR_MESH_H
#define SR_MESH_H

float tesselatebezier(int degree, float t, float a, float b, float c, float d);
float tesselatebezierderivative(int degree, float t, float a, float b, float c, float d);
void addbeziersurface(const float *points, const float *uvs, int steps_u, int degree_u,
                     int steps_v, int degree_v, int flags);
void addbeziersurfacetexture(const float *points, const float *uvs, int steps_u, int degree_u,
                            int steps_v, int degree_v, int flags);

#endif
