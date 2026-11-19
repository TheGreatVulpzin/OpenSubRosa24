/* The free places of the sector mesh buffer. See sector_marks.c. */
#ifndef CL_RENDER_SECTOR_MARKS_H
#define CL_RENDER_SECTOR_MARKS_H

#include "sr_types.h"

void __cdecl area_setsectormesh(int place, int value);
int __cdecl area_getnextsectormesh(void);
void __cdecl area_removesectormodel(int sector);

#endif /* CL_RENDER_SECTOR_MARKS_H */
