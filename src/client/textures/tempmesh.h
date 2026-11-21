/* Joins of the quads of the temporary mesh. See tempmesh.c. */
#ifndef CL_TEXTURES_TEMPMESH_H
#define CL_TEXTURES_TEMPMESH_H

void __cdecl tempmesh_optimize(void);
void __cdecl tempmesh_optimize2(void);

int __cdecl tempmesh_quadnormalequal(int a, int b);

#endif /* CL_TEXTURES_TEMPMESH_H */
