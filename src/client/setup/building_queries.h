/* Questions about the floor plan and the material names. See building_queries.c. */
#ifndef CL_SETUP_BUILDING_QUERIES_H
#define CL_SETUP_BUILDING_QUERIES_H

#include "sr_types.h"

int __cdecl building_iswindow(int column, int floor, int row, int side, int door);
void __cdecl building_reloadtextures(void);

#endif /* CL_SETUP_BUILDING_QUERIES_H */
