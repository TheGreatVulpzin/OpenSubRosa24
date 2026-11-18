/* Fixed mesh blocks for the outer faces of sectors. */
#ifndef CL_RENDER_SECTOR_MODELS_H
#define CL_RENDER_SECTOR_MODELS_H

int area_getnextsectormesh(void);
void area_setsectormesh(int first, int used);
void area_removesectormodel(int sector);
void area_submitsectormodel(int sector);
void area_resetsector(int sector);
int area_sectorvertequal(int sector, int first, int second);

#endif
