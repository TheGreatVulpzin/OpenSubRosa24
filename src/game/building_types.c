/* Colors and openings for building parts. */
#include "buildings.h"

#include <string.h>

#define g_buildingtype sr_data_buildingtype
#define g_buildblocktype sr_data_buildblocktype
#define g_teamcolor sr_data_teamcolor

void setupbuildingtypes(void)
{
    static const uint32_t trim_color[3] = {0x3f33b3b4, 0x3ecccccd, 0x3e24a4a5};
    static const char *const names[4] = {"test", "test2", "glassonly", "parking"};
    static const int counts[4] = {5, 4, 4, 5};
    static const BuildPieceCopy pieces[4][5] = {
        {{0, 0.125f, 0.75f, 0.125f, 0.5f, 15},
         {0, 0.125f, 0.75f, 0.125f, 0.875f, 7},
         {0, 0.125f, 0.75f, 0.0f, 0.625f, 11},
         {0, 0.25f, 0.25f, 0.125f, 0.5f, 15},
         {0, 0.25f, 0.5f, 0.25f, 0.375f, 15}},
        {{0, 0.125f, 0.75f, 0.125f, 0.5f, 15},
         {0, 0.125f, 0.875f, 0.125f, 0.625f, 13},
         {0, 0.0f, 0.875f, 0.125f, 0.625f, 14},
         {0, 0.0f, 1.0f, 0.125f, 0.625f, 12}},
        {{0, 0.0f, 1.0f, 0.0f, 1.0f, 0},
         {0, 0.125f, 0.875f, 0.0f, 1.0f, 1},
         {0, 0.0f, 0.875f, 0.0f, 1.0f, 2},
         {0, 0.125f, 0.75f, 0.0f, 1.0f, 3}},
        {{1, 0.0f, 1.0f, 0.1875f, 0.3125f, 12},
         {1, 0.125f, 0.875f, 0.1875f, 0.3125f, 13},
         {1, 0.0f, 0.75f, 0.1875f, 0.3125f, 14},
         {1, 0.125f, 0.875f, 0.0f, 0.875f, 1},
         {1, 0.0f, 0.875f, 0.0f, 0.875f, 2}},
    };
    BuildingType *bt = &g_buildingtype;
    int i, rec, piece;
    for (i = 0; i < 64; i++)
        bt->parts[i].colors[0] = (vec3){1.0f, 1.0f, 1.0f};
    memcpy(&bt->parts[0].colors[1], trim_color, sizeof trim_color);
    bt->parts[0].colors[2] = (vec3){0.125f, 0.125f, 0.125f};
    memcpy(bt->parts[0].team_colors, g_teamcolor, sizeof bt->parts[0].team_colors);
    for (rec = 0; rec < 4; rec++) {
        strcpy(g_buildblocktype[rec].name, names[rec]);
        for (piece = 0; piece < counts[rec]; piece++)
            memcpy(&g_buildblocktype[rec].pieces[piece], &pieces[rec][piece], sizeof(BuildPieceCopy));
    }
    bt->num_pieces = 0;
    for (rec = 0; rec < 4; rec++) {
        for (piece = 0; piece < 8; piece++) {
            BuildPiece *src = &g_buildblocktype[rec].pieces[piece];
            int n = bt->num_pieces;
            memcpy(&bt->pieces[n], src, sizeof(BuildPieceCopy));
            src->index = n;
            bt->num_pieces++;
        }
    }
}
