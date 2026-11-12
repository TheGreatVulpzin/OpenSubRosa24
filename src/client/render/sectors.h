/* The interior passes of a block sector. See sectors.c. */
#ifndef CL_RENDER_SECTORS_H
#define CL_RENDER_SECTORS_H

void __cdecl render_sector_interior(int sector, int mode);
void __cdecl render_sector_alpha(int sector, int mode);
void __cdecl render_sector_interior_shadow2(int sector);

#endif /* CL_RENDER_SECTORS_H */
