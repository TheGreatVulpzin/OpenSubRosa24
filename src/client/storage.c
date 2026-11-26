/* The data of the client: one C object for each datum of the game. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_runtime.h"
#include <string.h>

/* The data of the game. Each object has the size that the clean code expects. */
#ifdef SR_STORAGE_GUARDS
struct cl_hold_hack_modulusopt cl_hold_hack_modulusopt;
#else
int sr_data_hack_modulusopt[3];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_hack_modulusopt) == 3 * 4, "sr_data_hack_modulusopt");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubevertex cl_hold_cubevertex;
#else
vec3 sr_data_cubevertex[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubevertex) == 8 * 12, "sr_data_cubevertex");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubenormal cl_hold_cubenormal;
#else
vec3 sr_data_cubenormal[6];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubenormal) == 6 * 12, "sr_data_cubenormal");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubetangent cl_hold_cubetangent;
#else
vec3 sr_data_cubetangent[6];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubetangent) == 6 * 12, "sr_data_cubetangent");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubebinormal cl_hold_cubebinormal;
#else
vec3 sr_data_cubebinormal[6];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubebinormal) == 6 * 12, "sr_data_cubebinormal");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubetexcoord cl_hold_cubetexcoord;
#else
float sr_data_cubetexcoord[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubetexcoord) == 8 * 4, "sr_data_cubetexcoord");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubequad cl_hold_cubequad;
#else
int sr_data_cubequad[24];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubequad) == 24 * 4, "sr_data_cubequad");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubetri cl_hold_cubetri;
#else
int sr_data_cubetri[36];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubetri) == 36 * 4, "sr_data_cubetri");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cubeline cl_hold_cubeline;
#else
int sr_data_cubeline[24];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cubeline) == 24 * 4, "sr_data_cubeline");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_octahedron cl_hold_octahedron;
#else
float sr_data_octahedron[18];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_octahedron) == 18 * 4, "sr_data_octahedron");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_octahedronindex cl_hold_octahedronindex;
#else
int sr_data_octahedronindex[24];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_octahedronindex) == 24 * 4, "sr_data_octahedronindex");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_debug_cubevertex cl_hold_debug_cubevertex;
#else
float sr_data_debug_cubevertex[24];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_debug_cubevertex) == 24 * 4, "sr_data_debug_cubevertex");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_currentversion cl_hold_currentversion;
#else
int sr_data_currentversion;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_currentversion) == 1 * 4, "sr_data_currentversion");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_teamcolor cl_hold_teamcolor;
#else
float sr_data_teamcolor[24];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_teamcolor) == 24 * 4, "sr_data_teamcolor");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_teamname cl_hold_teamname;
#else
const char * sr_data_teamname[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_teamname) == 8 * 4, "sr_data_teamname");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_dollarbillvalue cl_hold_dollarbillvalue;
#else
int sr_data_dollarbillvalue[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_dollarbillvalue) == 8 * 4, "sr_data_dollarbillvalue");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cslogo cl_hold_cslogo;
#else
uint8_t sr_data_cslogo[2048];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cslogo) == 2048 * 1, "sr_data_cslogo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menucolor cl_hold_menucolor;
#else
float sr_data_menucolor[3];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_menucolor) == 3 * 4, "sr_data_menucolor");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_csmstr cl_hold_csmstr;
#else
const char * sr_data_csmstr;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_csmstr) == 1 * 4, "sr_data_csmstr");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tracktrailmodelquad cl_hold_tracktrailmodelquad;
#else
int sr_data_tracktrailmodelquad[12];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_tracktrailmodelquad) == 12 * 4, "sr_data_tracktrailmodelquad");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_carcolorname cl_hold_carcolorname;
#else
const char * sr_data_carcolorname[6];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_carcolorname) == 6 * 4, "sr_data_carcolorname");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_mti cl_hold_mti;
#else
int sr_data_mti;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_mti) == 1 * 4, "sr_data_mti");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_xaxis cl_hold_xaxis;
#else
vec3 sr_data_xaxis;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_xaxis) == 1 * 12, "sr_data_xaxis");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_yaxis cl_hold_yaxis;
#else
vec3 sr_data_yaxis;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_yaxis) == 1 * 12, "sr_data_yaxis");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_zaxis cl_hold_zaxis;
#else
vec3 sr_data_zaxis;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_zaxis) == 1 * 12, "sr_data_zaxis");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_socketid cl_hold_socketid;
#else
uint32_t sr_data_socketid;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_socketid) == 1 * 4, "sr_data_socketid");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_filtered_word cl_hold_filtered_word;
#else
char sr_data_filtered_word[7];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_filtered_word) == 7 * 1, "sr_data_filtered_word");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_mag01 cl_hold_mag01;
#else
uint32_t sr_data_mag01[2];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_mag01) == 2 * 4, "sr_data_mag01");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_newspaper_line cl_hold_newspaper_line;
#else
char sr_data_newspaper_line[128];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_newspaper_line) == 128 * 1, "sr_data_newspaper_line");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tzset_done cl_hold_tzset_done;
#else
int sr_data_tzset_done;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_tzset_done) == 1 * 4, "sr_data_tzset_done");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_mt cl_hold_mt;
#else
uint32_t sr_data_mt[624];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_mt) == 624 * 4, "sr_data_mt");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_manager_msg cl_hold_manager_msg;
#else
char sr_data_manager_msg[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_manager_msg) == 64 * 1, "sr_data_manager_msg");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_world_msg cl_hold_world_msg;
#else
char sr_data_world_msg[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_world_msg) == 64 * 1, "sr_data_world_msg");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_message cl_hold_message;
#else
char sr_data_message[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_message) == 64 * 1, "sr_data_message");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_farbs_digits cl_hold_farbs_digits;
#else
char sr_data_farbs_digits[128];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_farbs_digits) == 128 * 1, "sr_data_farbs_digits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_guard_used cl_hold_guard_used;
#else
int sr_data_guard_used[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_guard_used) == 64 * 4, "sr_data_guard_used");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_world_digits cl_hold_world_digits;
#else
char sr_data_world_digits[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_world_digits) == 64 * 1, "sr_data_world_digits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_newspaper_money cl_hold_newspaper_money;
#else
char sr_data_newspaper_money[128];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_newspaper_money) == 128 * 1, "sr_data_newspaper_money");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_farbs_line cl_hold_farbs_line;
#else
char sr_data_farbs_line[128];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_farbs_line) == 128 * 1, "sr_data_farbs_line");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_debugit cl_hold_debugit;
#else
int sr_data_debugit;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_debugit) == 1 * 4, "sr_data_debugit");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_dedicated cl_hold_dedicated;
#else
int sr_data_dedicated;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_dedicated) == 1 * 4, "sr_data_dedicated");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_floorplan cl_hold_floorplan;
#else
FloorplanCell sr_data_floorplan[262144];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_floorplan) == 262144 * 72, "sr_data_floorplan");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_timer cl_hold_timer;
#else
Timer sr_data_timer[256];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_timer) == 256 * 88, "sr_data_timer");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bullet cl_hold_bullet;
#else
Bullet sr_data_bullet[16384];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bullet) == 16384 * 92, "sr_data_bullet");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_debug cl_hold_debug;
#else
Debug sr_data_debug;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_debug) == 1 * 336840, "sr_data_debug");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_human cl_hold_human;
#else
Human sr_data_human[512];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_human) == 512 * 11108, "sr_data_human");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_stats cl_hold_stats;
#else
Stats sr_data_stats;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_stats) == 1 * 8, "sr_data_stats");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_clientaccounts cl_hold_clientaccounts;
#else
SrClientAccountStore sr_data_clientaccounts;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_clientaccounts) == 1 * 1048580, "sr_data_clientaccounts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_pointtemp cl_hold_pointtemp;
#else
vec3 sr_data_pointtemp[512];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_pointtemp) == 512 * 12, "sr_data_pointtemp");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofconnections cl_hold_numofconnections;
#else
int sr_data_numofconnections;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofconnections) == 1 * 4, "sr_data_numofconnections");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofdynamicbonds cl_hold_numofdynamicbonds;
#else
int sr_data_numofdynamicbonds;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofdynamicbonds) == 1 * 4, "sr_data_numofdynamicbonds");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_surface2 cl_hold_surface2;
#else
SrSurface sr_data_surface2;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_surface2) == 1 * 320, "sr_data_surface2");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_building cl_hold_building;
#else
Building sr_data_building;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_building) == 1 * 56715512, "sr_data_building");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_blocktemp cl_hold_blocktemp;
#else
BlockTemp sr_data_blocktemp;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_blocktemp) == 1 * 483436, "sr_data_blocktemp");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_object cl_hold_object;
#else
Object sr_data_object[1024];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_object) == 1024 * 33328, "sr_data_object");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofsprites cl_hold_numofsprites;
#else
int sr_data_numofsprites;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofsprites) == 1 * 4, "sr_data_numofsprites");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bezier_curve cl_hold_bezier_curve;
#else
float sr_data_bezier_curve[12];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bezier_curve) == 12 * 4, "sr_data_bezier_curve");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_server cl_hold_server;
#else
Server sr_data_server;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_server) == 1 * 132, "sr_data_server");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_points cl_hold_sprite_points;
#else
int sr_data_sprite_points;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_sprite_points) == 1 * 4, "sr_data_sprite_points");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_lines cl_hold_sprite_lines;
#else
int sr_data_sprite_lines;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_sprite_lines) == 1 * 4, "sr_data_sprite_lines");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_quads cl_hold_sprite_quads;
#else
int sr_data_sprite_quads;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_sprite_quads) == 1 * 4, "sr_data_sprite_quads");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_triangles cl_hold_sprite_triangles;
#else
int sr_data_sprite_triangles;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_sprite_triangles) == 1 * 4, "sr_data_sprite_triangles");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_buildingtype cl_hold_buildingtype;
#else
BuildingType sr_data_buildingtype;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_buildingtype) == 1 * 16832516, "sr_data_buildingtype");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_option cl_hold_option;
#else
OptionClient sr_data_option;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_option) == 1 * 336, "sr_data_option");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numoftracks cl_hold_numoftracks;
#else
int sr_data_numoftracks;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numoftracks) == 1 * 4, "sr_data_numoftracks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game cl_hold_game;
#else
GameClient sr_data_game;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_game) == 1 * 154976, "sr_data_game");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_file_read cl_hold_file_read;
#else
SrFileTransfer sr_data_file_read;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_file_read) == 1 * 4, "sr_data_file_read");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_item cl_hold_item;
#else
Item sr_data_item[1024];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_item) == 1024 * 428, "sr_data_item");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_random_counter cl_hold_random_counter;
#else
uint32_t sr_data_random_counter;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_random_counter) == 1 * 4, "sr_data_random_counter");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sysinfo cl_hold_sysinfo;
#else
SysInfo sr_data_sysinfo;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_sysinfo) == 1 * 1048, "sr_data_sysinfo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_soundlist cl_hold_soundlist;
#else
SoundList sr_data_soundlist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_soundlist) == 1 * 316, "sr_data_soundlist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_event cl_hold_event;
#else
Event sr_data_event[65536];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_event) == 65536 * 128, "sr_data_event");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_newspaper cl_hold_newspaper;
#else
Newspaper sr_data_newspaper;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_newspaper) == 1 * 262152, "sr_data_newspaper");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_landscape cl_hold_landscape;
#else
SrLandscape sr_data_landscape;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_landscape) == 1 * 16793616, "sr_data_landscape");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_wsadata cl_hold_wsadata;
#else
uint8_t sr_data_wsadata[400];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_wsadata) == 400 * 1, "sr_data_wsadata");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_scenario cl_hold_scenario;
#else
Scenario sr_data_scenario;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_scenario) == 1 * 308, "sr_data_scenario");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_objecttype cl_hold_objecttype;
#else
ObjectType sr_data_objecttype[128];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_objecttype) == 128 * 33420, "sr_data_objecttype");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bezier_curve_dv cl_hold_bezier_curve_dv;
#else
float sr_data_bezier_curve_dv[12];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bezier_curve_dv) == 12 * 4, "sr_data_bezier_curve_dv");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_particle cl_hold_particle;
#else
Particle sr_data_particle[16384];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_particle) == 16384 * 240, "sr_data_particle");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_buildblocktype cl_hold_buildblocktype;
#else
BuildBlockTypeRec sr_data_buildblocktype[16];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_buildblocktype) == 16 * 424, "sr_data_buildblocktype");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tiremarkend cl_hold_tiremarkend;
#else
int sr_data_tiremarkend;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_tiremarkend) == 1 * 4, "sr_data_tiremarkend");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_blocklist cl_hold_blocklist;
#else
BlockList sr_data_blocklist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_blocklist) == 1 * 262996, "sr_data_blocklist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tiremarkstart cl_hold_tiremarkstart;
#else
int sr_data_tiremarkstart;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_tiremarkstart) == 1 * 4, "sr_data_tiremarkstart");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_surface cl_hold_surface;
#else
SrSurface sr_data_surface;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_surface) == 1 * 320, "sr_data_surface");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_interior cl_hold_interior;
#else
SrInteriorStore sr_data_interior;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_interior) == 1 * 44236804, "sr_data_interior");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_filelist cl_hold_filelist;
#else
FileList sr_data_filelist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_filelist) == 1 * 524320, "sr_data_filelist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_city cl_hold_city;
#else
City sr_data_city;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_city) == 1 * 10533276, "sr_data_city");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_voice cl_hold_voice;
#else
VoiceChannelClient sr_data_voice[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_voice) == 8 * 168524, "sr_data_voice");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_player cl_hold_player;
#else
Player sr_data_player[256];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_player) == 256 * 9224, "sr_data_player");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cityobjecttype cl_hold_cityobjecttype;
#else
CityObjectTypeRec sr_data_cityobjecttype[256];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cityobjecttype) == 256 * 9240, "sr_data_cityobjecttype");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_materiallist cl_hold_materiallist;
#else
MaterialList sr_data_materiallist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_materiallist) == 1 * 1408, "sr_data_materiallist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_body cl_hold_body;
#else
Body sr_data_body[32];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_body) == 32 * 64, "sr_data_body");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_level_editor cl_hold_level_editor;
#else
LevelEditor sr_data_level_editor;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_level_editor) == 1 * 1861784, "sr_data_level_editor");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofevents cl_hold_numofevents;
#else
uint32_t sr_data_numofevents;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofevents) == 1 * 4, "sr_data_numofevents");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bullettypes cl_hold_bullettypes;
#else
BulletType sr_data_bullettypes[4];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bullettypes) == 4 * 24, "sr_data_bullettypes");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_camera cl_hold_camera;
#else
float sr_data_camera[130];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_camera) == 130 * 4, "sr_data_camera");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_accounts cl_hold_accounts;
#else
SrAccountStore sr_data_accounts;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_accounts) == 1 * 4980744, "sr_data_accounts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bondopt cl_hold_bondopt;
#else
BondOpt sr_data_bondopt[16384];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bondopt) == 16384 * 64, "sr_data_bondopt");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_radix cl_hold_radix;
#else
Radix sr_data_radix;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_radix) == 1 * 4196356, "sr_data_radix");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_intersect cl_hold_intersect;
#else
Intersect sr_data_intersect;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_intersect) == 1 * 132, "sr_data_intersect");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_slogfp cl_hold_slogfp;
#else
void * sr_data_slogfp;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_slogfp) == 1 * 4, "sr_data_slogfp");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_level_marks cl_hold_level_marks;
#else
uint8_t sr_data_level_marks[4096];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_level_marks) == 4096 * 1, "sr_data_level_marks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofsprite_models cl_hold_numofsprite_models;
#else
int sr_data_numofsprite_models;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofsprite_models) == 1 * 4, "sr_data_numofsprite_models");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_timerlist cl_hold_timerlist;
#else
TimerListClient sr_data_timerlist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_timerlist) == 1 * 84, "sr_data_timerlist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_speex cl_hold_speex;
#else
Speex sr_data_speex;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_speex) == 1 * 9637888, "sr_data_speex");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_area_block cl_hold_area_block;
#else
AreaBlock sr_data_area_block[65536];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_area_block) == 65536 * 132, "sr_data_area_block");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_file_write cl_hold_file_write;
#else
SrFileTransfer sr_data_file_write;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_file_write) == 1 * 4, "sr_data_file_write");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofstaticbonds cl_hold_numofstaticbonds;
#else
int sr_data_numofstaticbonds;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofstaticbonds) == 1 * 4, "sr_data_numofstaticbonds");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bezier_curve_uv cl_hold_bezier_curve_uv;
#else
float sr_data_bezier_curve_uv[8];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bezier_curve_uv) == 8 * 4, "sr_data_bezier_curve_uv");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_parser cl_hold_parser;
#else
Parser sr_data_parser;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_parser) == 1 * 1048844, "sr_data_parser");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_texturelist cl_hold_texturelist;
#else
TextureList sr_data_texturelist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_texturelist) == 1 * 4420, "sr_data_texturelist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cityobject cl_hold_cityobject;
#else
CityObjectRec sr_data_cityobject[1024];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_cityobject) == 1024 * 84, "sr_data_cityobject");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_connection cl_hold_connection;
#else
Connection sr_data_connection[256];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_connection) == 256 * 61808, "sr_data_connection");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_materials cl_hold_materials;
#else
SrMaterialStoreClient sr_data_materials;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_materials) == 1 * 90112, "sr_data_materials");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_area cl_hold_area;
#else
Area sr_data_area[4];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_area) == 4 * 1052012, "sr_data_area");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_market cl_hold_market;
#else
Market sr_data_market;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_market) == 1 * 516, "sr_data_market");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bondparticle cl_hold_bondparticle;
#else
BondParticle sr_data_bondparticle[16384];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bondparticle) == 16384 * 32, "sr_data_bondparticle");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bondpass cl_hold_bondpass;
#else
int sr_data_bondpass;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bondpass) == 1 * 4, "sr_data_bondpass");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_masterserver cl_hold_masterserver;
#else
MasterServer sr_data_masterserver;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_masterserver) == 1 * 520, "sr_data_masterserver");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofbonds cl_hold_numofbonds;
#else
int sr_data_numofbonds;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofbonds) == 1 * 4, "sr_data_numofbonds");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bond cl_hold_bond;
#else
Bond sr_data_bond[16384];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bond) == 16384 * 236, "sr_data_bond");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofbullets cl_hold_numofbullets;
#else
int sr_data_numofbullets;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofbullets) == 1 * 4, "sr_data_numofbullets");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_config cl_hold_config;
#else
Config sr_data_config;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_config) == 1 * 36, "sr_data_config");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_modellist cl_hold_modellist;
#else
ModelList sr_data_modellist;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_modellist) == 1 * 1956, "sr_data_modellist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_train_near cl_hold_train_near;
#else
int sr_data_train_near;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_train_near) == 1 * 4, "sr_data_train_near");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bsdsocket cl_hold_bsdsocket;
#else
BsdSocket sr_data_bsdsocket;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_bsdsocket) == 1 * 131184, "sr_data_bsdsocket");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_area_sector cl_hold_area_sector;
#else
SrSectorStore sr_data_area_sector;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_area_sector) == 1 * 202178624, "sr_data_area_sector");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game_world cl_hold_game_world;
#else
GameWorld sr_data_game_world;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_game_world) == 1 * 28, "sr_data_game_world");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tempmesh cl_hold_tempmesh;
#else
TempMesh sr_data_tempmesh;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_tempmesh) == 1 * 2955344, "sr_data_tempmesh");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_track cl_hold_track;
#else
TrackPiece sr_data_track[4096];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_track) == 4096 * 1400, "sr_data_track");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bondhuman_client cl_hold_bondhuman_client;
#else
BondHumanClient cl_data_bondhuman_client[512];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_bondhuman_client) == 512 * 1280, "cl_data_bondhuman_client");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_auth_ticket cl_hold_auth_ticket;
#else
uint8_t cl_data_auth_ticket[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_auth_ticket) == 1024 * 1, "cl_data_auth_ticket");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_auth_ticket_size cl_hold_auth_ticket_size;
#else
uint32_t cl_data_auth_ticket_size;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_auth_ticket_size) == 1 * 4, "cl_data_auth_ticket_size");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_auth_ticket_ready cl_hold_auth_ticket_ready;
#else
int cl_data_auth_ticket_ready;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_auth_ticket_ready) == 1 * 4, "cl_data_auth_ticket_ready");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_play_ticket cl_hold_play_ticket;
#else
int cl_data_play_ticket[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_play_ticket) == 4 * 4, "cl_data_play_ticket");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_play_ticket_on cl_hold_play_ticket_on;
#else
int cl_data_play_ticket_on;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_play_ticket_on) == 1 * 4, "cl_data_play_ticket_on");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_voice_test_samples cl_hold_voice_test_samples;
#else
int16_t cl_data_voice_test_samples[10240];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_voice_test_samples) == 10240 * 2, "cl_data_voice_test_samples");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_partial cl_hold_image_partial;
#else
int cl_data_image_partial;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_partial) == 1 * 4, "cl_data_image_partial");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_zlib_default_length cl_hold_zlib_default_length;
#else
uint8_t cl_data_zlib_default_length[288];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_zlib_default_length) == 288 * 1, "cl_data_zlib_default_length");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_zlib_default_distance cl_hold_zlib_default_distance;
#else
uint8_t cl_data_zlib_default_distance[32];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_zlib_default_distance) == 32 * 1, "cl_data_zlib_default_distance");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_error cl_hold_image_error;
#else
char* cl_data_image_error;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_error) == 1 * 4, "cl_data_image_error");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_fontnum cl_hold_fontnum;
#else
int cl_data_fontnum;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_fontnum) == 1 * 4, "cl_data_fontnum");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_fonts cl_hold_fonts;
#else
Font cl_data_fonts[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_fonts) == 4 * 9232, "cl_data_fonts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_font_bits cl_hold_font_bits;
#else
uint8_t cl_data_font_bits[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_font_bits) == 1024 * 1, "cl_data_font_bits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cursor_bits cl_hold_cursor_bits;
#else
uint8_t cl_data_cursor_bits[9];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_cursor_bits) == 9 * 1, "cl_data_cursor_bits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_loaders cl_hold_image_loaders;
#else
void* cl_data_image_loaders[32];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_loaders) == 32 * 4, "cl_data_image_loaders");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_loader_count cl_hold_image_loader_count;
#else
int cl_data_image_loader_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_loader_count) == 1 * 4, "cl_data_image_loader_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_hdr_gamma_inverse cl_hold_image_hdr_gamma_inverse;
#else
float cl_data_image_hdr_gamma_inverse;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_hdr_gamma_inverse) == 1 * 4, "cl_data_image_hdr_gamma_inverse");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_hdr_scale_inverse cl_hold_image_hdr_scale_inverse;
#else
float cl_data_image_hdr_scale_inverse;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_hdr_scale_inverse) == 1 * 4, "cl_data_image_hdr_scale_inverse");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_ldr_gamma cl_hold_image_ldr_gamma;
#else
float cl_data_image_ldr_gamma;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_ldr_gamma) == 1 * 4, "cl_data_image_ldr_gamma");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_ldr_scale cl_hold_image_ldr_scale;
#else
float cl_data_image_ldr_scale;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_ldr_scale) == 1 * 4, "cl_data_image_ldr_scale");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_wgl_loaders cl_hold_wgl_loaders;
#else
void * cl_data_wgl_loaders[37];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_wgl_loaders) == 37 * 4, "cl_data_wgl_loaders");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image_name cl_hold_image_name;
#else
char cl_data_image_name[128];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image_name) == 128 * 1, "cl_data_image_name");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sound_buffers cl_hold_sound_buffers;
#else
SoundBuffer cl_data_sound_buffers[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sound_buffers) == 256 * 12, "cl_data_sound_buffers");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_source_limit cl_hold_audio_source_limit;
#else
int cl_data_audio_source_limit;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_source_limit) == 1 * 4, "cl_data_audio_source_limit");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_source_buffer cl_hold_source_buffer;
#else
char cl_data_source_buffer[131072];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_source_buffer) == 131072 * 1, "cl_data_source_buffer");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_track_verts cl_hold_track_verts;
#else
ModelVertex cl_data_track_verts[917504];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_track_verts) == 917504 * 32, "cl_data_track_verts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_building_click_wait cl_hold_building_click_wait;
#else
int cl_data_building_click_wait;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_building_click_wait) == 1 * 4, "cl_data_building_click_wait");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_head_face_quads cl_hold_head_face_quads;
#else
int cl_data_head_face_quads[3];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_head_face_quads) == 3 * 4, "cl_data_head_face_quads");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_key_event_count cl_hold_key_event_count;
#else
int cl_data_key_event_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_key_event_count) == 1 * 4, "cl_data_key_event_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_key_event_next cl_hold_key_event_next;
#else
int cl_data_key_event_next;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_key_event_next) == 1 * 4, "cl_data_key_event_next");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_key_events cl_hold_key_events;
#else
KeyEvent cl_data_key_events[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_key_events) == 1024 * 12, "cl_data_key_events");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_message_delay cl_hold_message_delay;
#else
int cl_data_message_delay;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_message_delay) == 1 * 4, "cl_data_message_delay");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_num_joysticks cl_hold_num_joysticks;
#else
int cl_data_num_joysticks;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_num_joysticks) == 1 * 4, "cl_data_num_joysticks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_joysticks cl_hold_joysticks;
#else
Joystick cl_data_joysticks[16];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_joysticks) == 16 * 420, "cl_data_joysticks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_joysticks_previous cl_hold_joysticks_previous;
#else
Joystick cl_data_joysticks_previous[16];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_joysticks_previous) == 16 * 420, "cl_data_joysticks_previous");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_joystick_handles cl_hold_joystick_handles;
#else
void* cl_data_joystick_handles[16];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_joystick_handles) == 16 * 4, "cl_data_joystick_handles");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_back cl_hold_text_back;
#else
char cl_data_text_back[5];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_back) == 5 * 1, "cl_data_text_back");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_voice_minimum cl_hold_text_voice_minimum;
#else
char cl_data_text_voice_minimum[14];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_voice_minimum) == 14 * 1, "cl_data_text_voice_minimum");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_push_to_talk cl_hold_text_push_to_talk;
#else
char cl_data_text_push_to_talk[17];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_push_to_talk) == 17 * 1, "cl_data_text_push_to_talk");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_push_key cl_hold_text_push_key;
#else
char cl_data_text_push_key[17];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_push_key) == 17 * 1, "cl_data_text_push_key");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_voice_chat cl_hold_text_voice_chat;
#else
char cl_data_text_voice_chat[11];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_voice_chat) == 11 * 1, "cl_data_text_voice_chat");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_speaking cl_hold_text_speaking;
#else
char cl_data_text_speaking[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_speaking) == 2 * 1, "cl_data_text_speaking");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_level cl_hold_text_level;
#else
char cl_data_text_level[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_level) == 2 * 1, "cl_data_text_level");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_int_format cl_hold_text_int_format;
#else
char cl_data_text_int_format[3];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_int_format) == 3 * 1, "cl_data_text_int_format");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_bind_hint cl_hold_text_bind_hint;
#else
char cl_data_text_bind_hint[49];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_bind_hint) == 49 * 1, "cl_data_text_bind_hint");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_empty cl_hold_text_empty;
#else
char cl_data_text_empty;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_empty) == 1 * 1, "cl_data_text_empty");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_speex_frame_size cl_hold_text_speex_frame_size;
#else
char cl_data_text_speex_frame_size[15];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_speex_frame_size) == 15 * 1, "cl_data_text_speex_frame_size");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_bad_bmp cl_hold_text_bad_bmp;
#else
char cl_data_text_bad_bmp[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_bad_bmp) == 8 * 1, "cl_data_text_bad_bmp");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_text_png_unknown_chunk cl_hold_text_png_unknown_chunk;
#else
char cl_data_text_png_unknown_chunk[21];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_text_png_unknown_chunk) == 21 * 1, "cl_data_text_png_unknown_chunk");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_atmo_constant cl_hold_atmo_constant;
#else
float cl_data_atmo_constant[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_atmo_constant) == 4 * 4, "cl_data_atmo_constant");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_atmo_wavelength cl_hold_atmo_wavelength;
#else
float cl_data_atmo_wavelength[3];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_atmo_wavelength) == 3 * 4, "cl_data_atmo_wavelength");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_block cl_hold_sector_block;
#else
int cl_data_sector_block;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_block) == 1 * 4, "cl_data_sector_block");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_app_ticket cl_hold_app_ticket;
#else
AppTicket cl_data_app_ticket;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_app_ticket) == 1 * 36, "cl_data_app_ticket");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_modeltemp cl_hold_modeltemp;
#else
ModelTemp cl_data_modeltemp;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_modeltemp) == 1 * 2555924, "cl_data_modeltemp");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_model cl_hold_model;
#else
Model cl_data_model[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_model) == 1024 * 3588, "cl_data_model");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_texture cl_hold_texture;
#else
Texture cl_data_texture[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_texture) == 4096 * 112, "cl_data_texture");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_image cl_hold_image;
#else
Image cl_data_image;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_image) == 1 * 67108872, "cl_data_image");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_landrender cl_hold_landrender;
#else
LandRender cl_data_landrender;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_landrender) == 1 * 290816, "cl_data_landrender");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bigendian cl_hold_bigendian;
#else
int cl_data_bigendian;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_bigendian) == 1 * 4, "cl_data_bigendian");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_landsector_vbo cl_hold_landsector_vbo;
#else
int cl_data_landsector_vbo[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_landsector_vbo) == 1024 * 4, "cl_data_landsector_vbo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_vbo cl_hold_vbo;
#else
VboRec cl_data_vbo[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_vbo) == 4096 * 8, "cl_data_vbo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_landsector_lod cl_hold_landsector_lod;
#else
LandSectorLod cl_data_landsector_lod[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_landsector_lod) == 1024 * 20, "cl_data_landsector_lod");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_fontlist cl_hold_fontlist;
#else
int cl_data_fontlist[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_fontlist) == 2 * 4, "cl_data_fontlist");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sphere cl_hold_sphere;
#else
SphereLevel cl_data_sphere[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sphere) == 8 * 16, "cl_data_sphere");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_texture_windowatlas cl_hold_texture_windowatlas;
#else
WindowAtlas cl_data_texture_windowatlas;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_texture_windowatlas) == 1 * 516, "cl_data_texture_windowatlas");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_texture_human cl_hold_texture_human;
#else
HumanLook cl_data_texture_human[512];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_texture_human) == 512 * 20, "cl_data_texture_human");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_bonevert cl_hold_bonevert;
#else
BoneVert cl_data_bonevert[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_bonevert) == 4096 * 28, "cl_data_bonevert");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_cornervert cl_hold_cornervert;
#else
BoneVert cl_data_cornervert[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_cornervert) == 4096 * 28, "cl_data_cornervert");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_draw_text cl_hold_draw_text;
#else
char cl_data_draw_text[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_draw_text) == 1024 * 1, "cl_data_draw_text");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_draw_number cl_hold_draw_number;
#else
char cl_data_draw_number[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_draw_number) == 1024 * 1, "cl_data_draw_number");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_glext cl_hold_glext;
#else
int cl_data_glext[17];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_glext) == 17 * 4, "cl_data_glext");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_programs cl_hold_programs;
#else
ProgramObject cl_data_programs[32];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_programs) == 32 * 80, "cl_data_programs");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_shader_log cl_hold_shader_log;
#else
char cl_data_shader_log[65536];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_shader_log) == 65536 * 1, "cl_data_shader_log");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_info_log cl_hold_info_log;
#else
char cl_data_info_log[65536];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_info_log) == 65536 * 1, "cl_data_info_log");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_render_width cl_hold_render_width;
#else
int cl_data_render_width;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_render_width) == 1 * 4, "cl_data_render_width");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_render_height cl_hold_render_height;
#else
int cl_data_render_height;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_render_height) == 1 * 4, "cl_data_render_height");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_open_file cl_hold_open_file;
#else
int cl_data_open_file;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_open_file) == 1 * 4, "cl_data_open_file");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_light_position cl_hold_light_position;
#else
float cl_data_light_position[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_light_position) == 4 * 4, "cl_data_light_position");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_world_position cl_hold_world_position;
#else
vec3 cl_data_world_position;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_world_position) == 1 * 12, "cl_data_world_position");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_viewport_width cl_hold_viewport_width;
#else
float cl_data_viewport_width;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_viewport_width) == 1 * 4, "cl_data_viewport_width");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_viewport_height cl_hold_viewport_height;
#else
float cl_data_viewport_height;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_viewport_height) == 1 * 4, "cl_data_viewport_height");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_track_vbo cl_hold_track_vbo;
#else
int cl_data_track_vbo;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_track_vbo) == 1 * 4, "cl_data_track_vbo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_track_vertex_floats cl_hold_track_vertex_floats;
#else
int cl_data_track_vertex_floats;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_track_vertex_floats) == 1 * 4, "cl_data_track_vertex_floats");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_track_vertex_count cl_hold_track_vertex_count;
#else
int cl_data_track_vertex_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_track_vertex_count) == 1 * 4, "cl_data_track_vertex_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_frustum_planes cl_hold_frustum_planes;
#else
vec3 cl_data_frustum_planes[5];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_frustum_planes) == 5 * 12, "cl_data_frustum_planes");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_shadow_corners cl_hold_shadow_corners;
#else
vec3 cl_data_shadow_corners[6];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_shadow_corners) == 6 * 12, "cl_data_shadow_corners");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_shadow_count cl_hold_shadow_count;
#else
int cl_data_shadow_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_shadow_count) == 1 * 4, "cl_data_shadow_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_shadow_list cl_hold_shadow_list;
#else
vec3 cl_data_shadow_list[1048576];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_shadow_list) == 1048576 * 12, "cl_data_shadow_list");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_chat_head cl_hold_chat_head;
#else
uint32_t cl_data_chat_head;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_chat_head) == 1 * 4, "cl_data_chat_head");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_models cl_hold_sprite_models;
#else
SpriteModel cl_data_sprite_models[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sprite_models) == 16384 * 104, "cl_data_sprite_models");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_shell_verts cl_hold_shell_verts;
#else
SpriteVertex cl_data_shell_verts[262144];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_shell_verts) == 262144 * 32, "cl_data_shell_verts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_debris_verts cl_hold_debris_verts;
#else
SpriteVertex cl_data_debris_verts[65536];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_debris_verts) == 65536 * 32, "cl_data_debris_verts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_score_count cl_hold_score_count;
#else
int cl_data_score_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_score_count) == 1 * 4, "cl_data_score_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_score_entries cl_hold_score_entries;
#else
ScoreEntry cl_data_score_entries[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_score_entries) == 256 * 8, "cl_data_score_entries");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_display_text cl_hold_display_text;
#else
char cl_data_display_text[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_display_text) == 256 * 1, "cl_data_display_text");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_light_ambient cl_hold_light_ambient;
#else
float cl_data_light_ambient[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_light_ambient) == 4 * 4, "cl_data_light_ambient");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_interior_corners cl_hold_interior_corners;
#else
vec3 cl_data_interior_corners[6];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_interior_corners) == 6 * 12, "cl_data_interior_corners");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_part_count cl_hold_sector_part_count;
#else
int cl_data_sector_part_count[128];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_part_count) == 128 * 4, "cl_data_sector_part_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_draw_counts cl_hold_sector_draw_counts;
#else
int cl_data_sector_draw_counts[1048576];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_draw_counts) == 1048576 * 4, "cl_data_sector_draw_counts");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_draw_offsets cl_hold_sector_draw_offsets;
#else
void* cl_data_sector_draw_offsets[1048576];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_draw_offsets) == 1048576 * 4, "cl_data_sector_draw_offsets");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_count cl_hold_sector_count;
#else
int cl_data_sector_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_count) == 1 * 4, "cl_data_sector_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_list cl_hold_sector_list;
#else
int cl_data_sector_list[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_list) == 1024 * 4, "cl_data_sector_list");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_entries cl_hold_sector_entries;
#else
SectorEntry cl_data_sector_entries[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_entries) == 1024 * 8, "cl_data_sector_entries");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_interior_model_count cl_hold_interior_model_count;
#else
int cl_data_interior_model_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_interior_model_count) == 1 * 4, "cl_data_interior_model_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_vertices cl_hold_sector_vertices;
#else
uint8_t cl_data_sector_vertices[33554432];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_vertices) == 33554432 * 1, "cl_data_sector_vertices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_indices cl_hold_sector_indices;
#else
uint8_t cl_data_sector_indices[6291456];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_indices) == 6291456 * 1, "cl_data_sector_indices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_vbo cl_hold_sector_vbo;
#else
int cl_data_sector_vbo;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_vbo) == 1 * 4, "cl_data_sector_vbo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprites cl_hold_sprites;
#else
ClientSprite cl_data_sprites[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sprites) == 16384 * 148, "cl_data_sprites");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sprite_decals cl_hold_sprite_decals;
#else
SpriteDecalVertex cl_data_sprite_decals[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sprite_decals) == 16384 * 20, "cl_data_sprite_decals");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_glass_vertices cl_hold_glass_vertices;
#else
GlassVertex cl_data_glass_vertices[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_glass_vertices) == 16384 * 24, "cl_data_glass_vertices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_tiremarks cl_hold_tiremarks;
#else
Tiremark cl_data_tiremarks[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_tiremarks) == 16384 * 68, "cl_data_tiremarks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_selected_item cl_hold_selected_item;
#else
int cl_data_selected_item;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_selected_item) == 1 * 4, "cl_data_selected_item");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_shadow_corners cl_hold_sector_shadow_corners;
#else
vec3 cl_data_sector_shadow_corners[6];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_shadow_corners) == 6 * 12, "cl_data_sector_shadow_corners");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_shadow_count cl_hold_sector_shadow_count;
#else
int cl_data_sector_shadow_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_shadow_count) == 1 * 4, "cl_data_sector_shadow_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_shadow_vertices cl_hold_sector_shadow_vertices;
#else
vec3 cl_data_sector_shadow_vertices[262144];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_shadow_vertices) == 262144 * 12, "cl_data_sector_shadow_vertices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_shadow_buffer_ready cl_hold_sector_shadow_buffer_ready;
#else
int cl_data_sector_shadow_buffer_ready;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_shadow_buffer_ready) == 1 * 4, "cl_data_sector_shadow_buffer_ready");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sector_shadow_vbo cl_hold_sector_shadow_vbo;
#else
int cl_data_sector_shadow_vbo;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sector_shadow_vbo) == 1 * 4, "cl_data_sector_shadow_vbo");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_frame_model_count cl_hold_frame_model_count;
#else
int cl_data_frame_model_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_frame_model_count) == 1 * 4, "cl_data_frame_model_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_frame_model_order cl_hold_frame_model_order;
#else
int cl_data_frame_model_order[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_frame_model_order) == 4096 * 4, "cl_data_frame_model_order");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_frame_models cl_hold_frame_models;
#else
FrameEntry cl_data_frame_models[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_frame_models) == 4096 * 100, "cl_data_frame_models");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_video_modes cl_hold_video_modes;
#else
VideoMode cl_data_video_modes[4096];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_video_modes) == 4096 * 12, "cl_data_video_modes");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_video_mode_count cl_hold_video_mode_count;
#else
int cl_data_video_mode_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_video_mode_count) == 1 * 4, "cl_data_video_mode_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_screenshot_pixels cl_hold_screenshot_pixels;
#else
uint32_t cl_data_screenshot_pixels[1228800];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_screenshot_pixels) == 1228800 * 4, "cl_data_screenshot_pixels");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_blur_params_near cl_hold_blur_params_near;
#else
float cl_data_blur_params_near[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_blur_params_near) == 8 * 4, "cl_data_blur_params_near");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_viewport_x cl_hold_viewport_x;
#else
float cl_data_viewport_x;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_viewport_x) == 1 * 4, "cl_data_viewport_x");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_viewport_y cl_hold_viewport_y;
#else
float cl_data_viewport_y;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_viewport_y) == 1 * 4, "cl_data_viewport_y");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_light_diffuse cl_hold_light_diffuse;
#else
float cl_data_light_diffuse[4];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_light_diffuse) == 4 * 4, "cl_data_light_diffuse");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_sky_color cl_hold_sky_color;
#else
float cl_data_sky_color[3];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_sky_color) == 3 * 4, "cl_data_sky_color");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_view_near cl_hold_view_near;
#else
float cl_data_view_near;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_view_near) == 1 * 4, "cl_data_view_near");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_view_far cl_hold_view_far;
#else
float cl_data_view_far;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_view_far) == 1 * 4, "cl_data_view_far");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_view_fov cl_hold_view_fov;
#else
float cl_data_view_fov;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_view_fov) == 1 * 4, "cl_data_view_fov");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_view_side_right cl_hold_view_side_right;
#else
vec3 cl_data_view_side_right;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_view_side_right) == 1 * 12, "cl_data_view_side_right");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_view_side_left cl_hold_view_side_left;
#else
vec3 cl_data_view_side_left;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_view_side_left) == 1 * 12, "cl_data_view_side_left");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_window_quad_count cl_hold_window_quad_count;
#else
int cl_data_window_quad_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_window_quad_count) == 1 * 4, "cl_data_window_quad_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_window_quads cl_hold_window_quads;
#else
WindowQuad cl_data_window_quads[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_window_quads) == 16384 * 68, "cl_data_window_quads");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_framebuffers cl_hold_framebuffers;
#else
ClientFramebuffer cl_data_framebuffers[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_framebuffers) == 2 * 8, "cl_data_framebuffers");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_framebuffer_index cl_hold_framebuffer_index;
#else
int cl_data_framebuffer_index;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_framebuffer_index) == 1 * 4, "cl_data_framebuffer_index");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_mouse cl_hold_input_mouse;
#else
InputMouse cl_data_input_mouse;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_mouse) == 1 * 44, "cl_data_input_mouse");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_mouse_previous cl_hold_input_mouse_previous;
#else
InputMouse cl_data_input_mouse_previous;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_mouse_previous) == 1 * 44, "cl_data_input_mouse_previous");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_cursor cl_hold_input_cursor;
#else
int cl_data_input_cursor;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_cursor) == 1 * 4, "cl_data_input_cursor");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_cursor_previous cl_hold_input_cursor_previous;
#else
int cl_data_input_cursor_previous;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_cursor_previous) == 1 * 4, "cl_data_input_cursor_previous");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_fire_blocked cl_hold_input_fire_blocked;
#else
int cl_data_input_fire_blocked;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_fire_blocked) == 1 * 4, "cl_data_input_fire_blocked");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_chat cl_hold_input_chat;
#else
int cl_data_input_chat;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_chat) == 1 * 4, "cl_data_input_chat");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_chat_head cl_hold_input_chat_head;
#else
uint32_t cl_data_input_chat_head;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_chat_head) == 1 * 4, "cl_data_input_chat_head");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_chat_length cl_hold_input_chat_length;
#else
uint32_t cl_data_input_chat_length;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_chat_length) == 1 * 4, "cl_data_input_chat_length");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_chat_text cl_hold_input_chat_text;
#else
char cl_data_input_chat_text[64];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_chat_text) == 64 * 1, "cl_data_input_chat_text");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_kickedip cl_hold_kickedip;
#else
uint32_t sr_data_kickedip[1024];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_kickedip) == 1024 * 4, "sr_data_kickedip");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_numofkicked cl_hold_numofkicked;
#else
int sr_data_numofkicked;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_numofkicked) == 1 * 4, "sr_data_numofkicked");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_debug_toggle cl_hold_input_debug_toggle;
#else
uint32_t cl_data_input_debug_toggle;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_debug_toggle) == 1 * 4, "cl_data_input_debug_toggle");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_chat cl_hold_input_label_chat;
#else
char cl_data_input_label_chat;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_chat) == 1 * 1, "cl_data_input_label_chat");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_pause cl_hold_input_label_pause;
#else
char cl_data_input_label_pause[5];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_pause) == 5 * 1, "cl_data_input_label_pause");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_return cl_hold_input_label_return;
#else
char cl_data_input_label_return[15];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_return) == 15 * 1, "cl_data_input_label_return");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_options cl_hold_input_label_options;
#else
char cl_data_input_label_options[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_options) == 8 * 1, "cl_data_input_label_options");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_voice cl_hold_input_label_voice;
#else
char cl_data_input_label_voice[14];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_voice) == 14 * 1, "cl_data_input_label_voice");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_exit cl_hold_input_label_exit;
#else
char cl_data_input_label_exit[16];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_exit) == 16 * 1, "cl_data_input_label_exit");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_return_connections cl_hold_input_label_return_connections;
#else
char cl_data_input_label_return_connections[15];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_return_connections) == 15 * 1, "cl_data_input_label_return_connections");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_empty_connection cl_hold_input_label_empty_connection;
#else
char cl_data_input_label_empty_connection[5];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_empty_connection) == 5 * 1, "cl_data_input_label_empty_connection");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_human_axis cl_hold_input_label_human_axis;
#else
char cl_data_input_label_human_axis[10];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_human_axis) == 10 * 1, "cl_data_input_label_human_axis");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_timer_build cl_hold_input_label_timer_build;
#else
char cl_data_input_label_timer_build[14];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_timer_build) == 14 * 1, "cl_data_input_label_timer_build");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_timer_optimize cl_hold_input_label_timer_optimize;
#else
char cl_data_input_label_timer_optimize[17];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_timer_optimize) == 17 * 1, "cl_data_input_label_timer_optimize");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_timer_setup cl_hold_input_label_timer_setup;
#else
char cl_data_input_label_timer_setup[14];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_timer_setup) == 14 * 1, "cl_data_input_label_timer_setup");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_timer_alloc cl_hold_input_label_timer_alloc;
#else
char cl_data_input_label_timer_alloc[14];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_timer_alloc) == 14 * 1, "cl_data_input_label_timer_alloc");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_input_label_timer_shadow cl_hold_input_label_timer_shadow;
#else
char cl_data_input_label_timer_shadow[15];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_input_label_timer_shadow) == 15 * 1, "cl_data_input_label_timer_shadow");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_objectpacket cl_hold_objectpacket;
#else
ObjectPacket sr_data_objectpacket;
#endif
SR_STATIC_ASSERT(sizeof(sr_data_objectpacket) == 1 * 52756, "sr_data_objectpacket");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_itemtype cl_hold_itemtype;
#else
ItemType sr_data_itemtype[64];
#endif
SR_STATIC_ASSERT(sizeof(sr_data_itemtype) == 64 * 408, "sr_data_itemtype");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_items cl_hold_menu_items;
#else
MenuItem cl_data_menu_items[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_items) == 256 * 644, "cl_data_menu_items");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_item_count cl_hold_menu_item_count;
#else
int cl_data_menu_item_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_item_count) == 1 * 4, "cl_data_menu_item_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_item_index cl_hold_menu_item_index;
#else
int cl_data_menu_item_index;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_item_index) == 1 * 4, "cl_data_menu_item_index");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_textinput_start cl_hold_textinput_start;
#else
int cl_data_textinput_start;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_textinput_start) == 1 * 4, "cl_data_textinput_start");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_textinput_end cl_hold_textinput_end;
#else
int cl_data_textinput_end;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_textinput_end) == 1 * 4, "cl_data_textinput_end");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_textinput_text cl_hold_textinput_text;
#else
char cl_data_textinput_text[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_textinput_text) == 16384 * 1, "cl_data_textinput_text");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_textinput_shown cl_hold_textinput_shown;
#else
char cl_data_textinput_shown[16384];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_textinput_shown) == 16384 * 1, "cl_data_textinput_shown");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_vertices cl_hold_menu_vertices;
#else
float cl_data_menu_vertices[32];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_vertices) == 32 * 4, "cl_data_menu_vertices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_texcoords cl_hold_menu_texcoords;
#else
float cl_data_menu_texcoords[32];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_texcoords) == 32 * 4, "cl_data_menu_texcoords");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_alpha cl_hold_menu_alpha;
#else
float cl_data_menu_alpha[16];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_alpha) == 16 * 4, "cl_data_menu_alpha");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_quad_count cl_hold_menu_quad_count;
#else
int cl_data_menu_quad_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_quad_count) == 1 * 4, "cl_data_menu_quad_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_menu_quads cl_hold_menu_quads;
#else
int cl_data_menu_quads[64];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_menu_quads) == 64 * 4, "cl_data_menu_quads");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_wrap_line_ends cl_hold_wrap_line_ends;
#else
int cl_data_wrap_line_ends[64];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_wrap_line_ends) == 64 * 4, "cl_data_wrap_line_ends");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_client_account_id cl_hold_client_account_id;
#else
int cl_data_client_account_id[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_client_account_id) == 2 * 4, "cl_data_client_account_id");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_textinput_overwrite cl_hold_textinput_overwrite;
#else
int cl_data_textinput_overwrite;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_textinput_overwrite) == 1 * 4, "cl_data_textinput_overwrite");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_wrap_line_count cl_hold_wrap_line_count;
#else
int cl_data_wrap_line_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_wrap_line_count) == 1 * 4, "cl_data_wrap_line_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_screen_bits cl_hold_screen_bits;
#else
int cl_data_screen_bits;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_screen_bits) == 1 * 4, "cl_data_screen_bits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_screen_fullscreen cl_hold_screen_fullscreen;
#else
int cl_data_screen_fullscreen;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_screen_fullscreen) == 1 * 4, "cl_data_screen_fullscreen");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_joystick_names cl_hold_joystick_names;
#else
char cl_data_joystick_names[2560];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_joystick_names) == 2560 * 1, "cl_data_joystick_names");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_servers cl_hold_join_servers;
#else
JoinServer cl_data_join_servers[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_servers) == 1024 * 72, "cl_data_join_servers");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_server_count cl_hold_join_server_count;
#else
int cl_data_join_server_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_server_count) == 1 * 4, "cl_data_join_server_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_addresses cl_hold_join_addresses;
#else
JoinAddress cl_data_join_addresses[8192];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_addresses) == 8192 * 8, "cl_data_join_addresses");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_address_count cl_hold_join_address_count;
#else
int cl_data_join_address_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_address_count) == 1 * 4, "cl_data_join_address_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_master_first cl_hold_join_master_first;
#else
int cl_data_join_master_first;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_master_first) == 1 * 4, "cl_data_join_master_first");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_join_columns cl_hold_join_columns;
#else
float cl_data_join_columns[10];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_join_columns) == 10 * 4, "cl_data_join_columns");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_vbo_next cl_hold_vbo_next;
#else
int cl_data_vbo_next;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_vbo_next) == 1 * 4, "cl_data_vbo_next");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_framebuffer_ready cl_hold_framebuffer_ready;
#else
int cl_data_framebuffer_ready;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_framebuffer_ready) == 1 * 4, "cl_data_framebuffer_ready");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_vbo_spare_a cl_hold_vbo_spare_a;
#else
int cl_data_vbo_spare_a;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_vbo_spare_a) == 1 * 4, "cl_data_vbo_spare_a");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_vbo_spare_b cl_hold_vbo_spare_b;
#else
int cl_data_vbo_spare_b;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_vbo_spare_b) == 1 * 4, "cl_data_vbo_spare_b");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_vbo_spare_c cl_hold_vbo_spare_c;
#else
int cl_data_vbo_spare_c;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_vbo_spare_c) == 1 * 4, "cl_data_vbo_spare_c");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_version_marks cl_hold_version_marks;
#else
char cl_data_version_marks[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_version_marks) == 2 * 1, "cl_data_version_marks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_device cl_hold_audio_device;
#else
void* cl_data_audio_device;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_device) == 1 * 4, "cl_data_audio_device");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_context cl_hold_audio_context;
#else
void* cl_data_audio_context;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_context) == 1 * 4, "cl_data_audio_context");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_capture cl_hold_audio_capture;
#else
void* cl_data_audio_capture;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_capture) == 1 * 4, "cl_data_audio_capture");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_enabled cl_hold_audio_enabled;
#else
int cl_data_audio_enabled;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_enabled) == 1 * 4, "cl_data_audio_enabled");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_encoder cl_hold_audio_encoder;
#else
AudioEncoder cl_data_audio_encoder;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_encoder) == 1 * 44, "cl_data_audio_encoder");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_decoders cl_hold_audio_decoders;
#else
AudioDecoder cl_data_audio_decoders[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_decoders) == 8 * 40, "cl_data_audio_decoders");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_camera cl_hold_audio_camera;
#else
AudioCamera cl_data_audio_camera;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_camera) == 1 * 68, "cl_data_audio_camera");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_sounds cl_hold_audio_sounds;
#else
AudioSound cl_data_audio_sounds[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_sounds) == 256 * 24, "cl_data_audio_sounds");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_sound_count cl_hold_audio_sound_count;
#else
int cl_data_audio_sound_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_sound_count) == 1 * 4, "cl_data_audio_sound_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_objects cl_hold_audio_objects;
#else
int cl_data_audio_objects[8];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_objects) == 8 * 4, "cl_data_audio_objects");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_object_count cl_hold_audio_object_count;
#else
int cl_data_audio_object_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_object_count) == 1 * 4, "cl_data_audio_object_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_play_buffer cl_hold_audio_play_buffer;
#else
int16_t cl_data_audio_play_buffer[65536];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_play_buffer) == 65536 * 2, "cl_data_audio_play_buffer");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_input_blocked cl_hold_audio_input_blocked;
#else
int cl_data_audio_input_blocked;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_input_blocked) == 1 * 4, "cl_data_audio_input_blocked");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_voice_mode cl_hold_audio_voice_mode;
#else
int cl_data_audio_voice_mode;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_voice_mode) == 1 * 4, "cl_data_audio_voice_mode");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_menu_back cl_hold_audio_menu_back;
#else
int cl_data_audio_menu_back;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_menu_back) == 1 * 4, "cl_data_audio_menu_back");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_window cl_hold_audio_window;
#else
void* cl_data_audio_window;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_window) == 1 * 4, "cl_data_audio_window");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_audio_key_names cl_hold_audio_key_names;
#else
char cl_data_audio_key_names[8192];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_audio_key_names) == 8192 * 1, "cl_data_audio_key_names");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_keyboard cl_hold_keyboard;
#else
uint8_t cl_data_keyboard[512];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_keyboard) == 512 * 1, "cl_data_keyboard");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_prevkeyboard cl_hold_prevkeyboard;
#else
uint8_t cl_data_prevkeyboard[512];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_prevkeyboard) == 512 * 1, "cl_data_prevkeyboard");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_chatmessage cl_hold_chatmessage;
#else
ChatMessage cl_data_chatmessage[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_chatmessage) == 256 * 152, "cl_data_chatmessage");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_render_aspect cl_hold_render_aspect;
#else
float cl_data_render_aspect;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_render_aspect) == 1 * 4, "cl_data_render_aspect");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_render_depth_bits cl_hold_render_depth_bits;
#else
int cl_data_render_depth_bits;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_render_depth_bits) == 1 * 4, "cl_data_render_depth_bits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_render_stencil_bits cl_hold_render_stencil_bits;
#else
int cl_data_render_stencil_bits;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_render_stencil_bits) == 1 * 4, "cl_data_render_stencil_bits");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_gl_context cl_hold_gl_context;
#else
void* cl_data_gl_context;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_gl_context) == 1 * 4, "cl_data_gl_context");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_steam_active cl_hold_steam_active;
#else
int cl_data_steam_active;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_steam_active) == 1 * 4, "cl_data_steam_active");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_player_marks cl_hold_player_marks;
#else
int cl_data_player_marks[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_player_marks) == 256 * 4, "cl_data_player_marks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_building_editor cl_hold_building_editor;
#else
BuildingEditor cl_data_building_editor;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_building_editor) == 1 * 236, "cl_data_building_editor");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_building_view cl_hold_building_view;
#else
EditorView cl_data_building_view;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_building_view) == 1 * 72, "cl_data_building_view");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_editor_tile_count cl_hold_editor_tile_count;
#else
int cl_data_editor_tile_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_editor_tile_count) == 1 * 4, "cl_data_editor_tile_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_editor_tiles cl_hold_editor_tiles;
#else
EditorTile cl_data_editor_tiles[64];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_editor_tiles) == 64 * 36, "cl_data_editor_tiles");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_edit_vertices cl_hold_edit_vertices;
#else
vec3 cl_data_edit_vertices[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_edit_vertices) == 256 * 12, "cl_data_edit_vertices");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_edit_grabbed cl_hold_edit_grabbed;
#else
int cl_data_edit_grabbed[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_edit_grabbed) == 256 * 4, "cl_data_edit_grabbed");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_edit_offsets cl_hold_edit_offsets;
#else
vec3 cl_data_edit_offsets[256];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_edit_offsets) == 256 * 12, "cl_data_edit_offsets");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_edit_mouse_before cl_hold_edit_mouse_before;
#else
float cl_data_edit_mouse_before[2];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_edit_mouse_before) == 2 * 4, "cl_data_edit_mouse_before");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game_block_count cl_hold_game_block_count;
#else
int cl_data_game_block_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_game_block_count) == 1 * 4, "cl_data_game_block_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game_blocks cl_hold_game_blocks;
#else
EditorEntry cl_data_game_blocks[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_game_blocks) == 1024 * 68, "cl_data_game_blocks");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game_texture_count cl_hold_game_texture_count;
#else
int cl_data_game_texture_count;
#endif
SR_STATIC_ASSERT(sizeof(cl_data_game_texture_count) == 1 * 4, "cl_data_game_texture_count");
#ifdef SR_STORAGE_GUARDS
struct cl_hold_game_textures cl_hold_game_textures;
#else
EditorEntry cl_data_game_textures[1024];
#endif
SR_STATIC_ASSERT(sizeof(cl_data_game_textures) == 1024 * 68, "cl_data_game_textures");

/* Objects that the client has no place for. */
char sr_unplaced_gamedir[9];
float sr_unplaced_lightwavelength[3];
float sr_unplaced_xaxis2d[2];
float sr_unplaced_yaxis2d[2];
BondHuman sr_unplaced_bondhuman[512];
int sr_unplaced_startup_counter;
SrPacketWorkspace sr_unplaced_packet_workspace;
int sr_client_end_time;
int sr_client_gametype;

#ifdef SR_STORAGE_GUARDS
#define CL_GUARD_SIZE 256
/* The guard after each object. A change in a guard means a write past the end of its object. */
static const struct { const char *name; unsigned char *guard; } cl_guards[] = {
    { "sr_data_hack_modulusopt", cl_hold_hack_modulusopt.guard },
    { "sr_data_cubevertex", cl_hold_cubevertex.guard },
    { "sr_data_cubenormal", cl_hold_cubenormal.guard },
    { "sr_data_cubetangent", cl_hold_cubetangent.guard },
    { "sr_data_cubebinormal", cl_hold_cubebinormal.guard },
    { "sr_data_cubetexcoord", cl_hold_cubetexcoord.guard },
    { "sr_data_cubequad", cl_hold_cubequad.guard },
    { "sr_data_cubetri", cl_hold_cubetri.guard },
    { "sr_data_cubeline", cl_hold_cubeline.guard },
    { "sr_data_octahedron", cl_hold_octahedron.guard },
    { "sr_data_octahedronindex", cl_hold_octahedronindex.guard },
    { "sr_data_debug_cubevertex", cl_hold_debug_cubevertex.guard },
    { "sr_data_currentversion", cl_hold_currentversion.guard },
    { "sr_data_teamcolor", cl_hold_teamcolor.guard },
    { "sr_data_teamname", cl_hold_teamname.guard },
    { "sr_data_dollarbillvalue", cl_hold_dollarbillvalue.guard },
    { "sr_data_cslogo", cl_hold_cslogo.guard },
    { "sr_data_menucolor", cl_hold_menucolor.guard },
    { "sr_data_csmstr", cl_hold_csmstr.guard },
    { "sr_data_tracktrailmodelquad", cl_hold_tracktrailmodelquad.guard },
    { "sr_data_carcolorname", cl_hold_carcolorname.guard },
    { "sr_data_mti", cl_hold_mti.guard },
    { "sr_data_xaxis", cl_hold_xaxis.guard },
    { "sr_data_yaxis", cl_hold_yaxis.guard },
    { "sr_data_zaxis", cl_hold_zaxis.guard },
    { "sr_data_socketid", cl_hold_socketid.guard },
    { "sr_data_filtered_word", cl_hold_filtered_word.guard },
    { "sr_data_mag01", cl_hold_mag01.guard },
    { "sr_data_newspaper_line", cl_hold_newspaper_line.guard },
    { "sr_data_tzset_done", cl_hold_tzset_done.guard },
    { "sr_data_mt", cl_hold_mt.guard },
    { "sr_data_manager_msg", cl_hold_manager_msg.guard },
    { "sr_data_world_msg", cl_hold_world_msg.guard },
    { "sr_data_message", cl_hold_message.guard },
    { "sr_data_farbs_digits", cl_hold_farbs_digits.guard },
    { "sr_data_guard_used", cl_hold_guard_used.guard },
    { "sr_data_world_digits", cl_hold_world_digits.guard },
    { "sr_data_newspaper_money", cl_hold_newspaper_money.guard },
    { "sr_data_farbs_line", cl_hold_farbs_line.guard },
    { "sr_data_debugit", cl_hold_debugit.guard },
    { "sr_data_dedicated", cl_hold_dedicated.guard },
    { "sr_data_floorplan", cl_hold_floorplan.guard },
    { "sr_data_timer", cl_hold_timer.guard },
    { "sr_data_bullet", cl_hold_bullet.guard },
    { "sr_data_debug", cl_hold_debug.guard },
    { "sr_data_human", cl_hold_human.guard },
    { "sr_data_stats", cl_hold_stats.guard },
    { "sr_data_clientaccounts", cl_hold_clientaccounts.guard },
    { "sr_data_pointtemp", cl_hold_pointtemp.guard },
    { "sr_data_numofconnections", cl_hold_numofconnections.guard },
    { "sr_data_numofdynamicbonds", cl_hold_numofdynamicbonds.guard },
    { "sr_data_surface2", cl_hold_surface2.guard },
    { "sr_data_building", cl_hold_building.guard },
    { "sr_data_blocktemp", cl_hold_blocktemp.guard },
    { "sr_data_object", cl_hold_object.guard },
    { "sr_data_numofsprites", cl_hold_numofsprites.guard },
    { "sr_data_bezier_curve", cl_hold_bezier_curve.guard },
    { "sr_data_server", cl_hold_server.guard },
    { "sr_data_sprite_points", cl_hold_sprite_points.guard },
    { "sr_data_sprite_lines", cl_hold_sprite_lines.guard },
    { "sr_data_sprite_quads", cl_hold_sprite_quads.guard },
    { "sr_data_sprite_triangles", cl_hold_sprite_triangles.guard },
    { "sr_data_buildingtype", cl_hold_buildingtype.guard },
    { "sr_data_option", cl_hold_option.guard },
    { "sr_data_numoftracks", cl_hold_numoftracks.guard },
    { "sr_data_game", cl_hold_game.guard },
    { "sr_data_file_read", cl_hold_file_read.guard },
    { "sr_data_item", cl_hold_item.guard },
    { "sr_data_random_counter", cl_hold_random_counter.guard },
    { "sr_data_sysinfo", cl_hold_sysinfo.guard },
    { "sr_data_soundlist", cl_hold_soundlist.guard },
    { "sr_data_event", cl_hold_event.guard },
    { "sr_data_newspaper", cl_hold_newspaper.guard },
    { "sr_data_landscape", cl_hold_landscape.guard },
    { "sr_data_wsadata", cl_hold_wsadata.guard },
    { "sr_data_scenario", cl_hold_scenario.guard },
    { "sr_data_objecttype", cl_hold_objecttype.guard },
    { "sr_data_bezier_curve_dv", cl_hold_bezier_curve_dv.guard },
    { "sr_data_particle", cl_hold_particle.guard },
    { "sr_data_buildblocktype", cl_hold_buildblocktype.guard },
    { "sr_data_tiremarkend", cl_hold_tiremarkend.guard },
    { "sr_data_blocklist", cl_hold_blocklist.guard },
    { "sr_data_tiremarkstart", cl_hold_tiremarkstart.guard },
    { "sr_data_surface", cl_hold_surface.guard },
    { "sr_data_interior", cl_hold_interior.guard },
    { "sr_data_filelist", cl_hold_filelist.guard },
    { "sr_data_city", cl_hold_city.guard },
    { "sr_data_voice", cl_hold_voice.guard },
    { "sr_data_player", cl_hold_player.guard },
    { "sr_data_cityobjecttype", cl_hold_cityobjecttype.guard },
    { "sr_data_materiallist", cl_hold_materiallist.guard },
    { "sr_data_body", cl_hold_body.guard },
    { "sr_data_level_editor", cl_hold_level_editor.guard },
    { "sr_data_numofevents", cl_hold_numofevents.guard },
    { "sr_data_bullettypes", cl_hold_bullettypes.guard },
    { "sr_data_camera", cl_hold_camera.guard },
    { "sr_data_accounts", cl_hold_accounts.guard },
    { "sr_data_bondopt", cl_hold_bondopt.guard },
    { "sr_data_radix", cl_hold_radix.guard },
    { "sr_data_intersect", cl_hold_intersect.guard },
    { "sr_data_slogfp", cl_hold_slogfp.guard },
    { "sr_data_level_marks", cl_hold_level_marks.guard },
    { "sr_data_numofsprite_models", cl_hold_numofsprite_models.guard },
    { "sr_data_timerlist", cl_hold_timerlist.guard },
    { "sr_data_speex", cl_hold_speex.guard },
    { "sr_data_area_block", cl_hold_area_block.guard },
    { "sr_data_file_write", cl_hold_file_write.guard },
    { "sr_data_numofstaticbonds", cl_hold_numofstaticbonds.guard },
    { "sr_data_bezier_curve_uv", cl_hold_bezier_curve_uv.guard },
    { "sr_data_parser", cl_hold_parser.guard },
    { "sr_data_texturelist", cl_hold_texturelist.guard },
    { "sr_data_cityobject", cl_hold_cityobject.guard },
    { "sr_data_connection", cl_hold_connection.guard },
    { "sr_data_materials", cl_hold_materials.guard },
    { "sr_data_area", cl_hold_area.guard },
    { "sr_data_market", cl_hold_market.guard },
    { "sr_data_bondparticle", cl_hold_bondparticle.guard },
    { "sr_data_bondpass", cl_hold_bondpass.guard },
    { "sr_data_masterserver", cl_hold_masterserver.guard },
    { "sr_data_numofbonds", cl_hold_numofbonds.guard },
    { "sr_data_bond", cl_hold_bond.guard },
    { "sr_data_numofbullets", cl_hold_numofbullets.guard },
    { "sr_data_config", cl_hold_config.guard },
    { "sr_data_modellist", cl_hold_modellist.guard },
    { "sr_data_train_near", cl_hold_train_near.guard },
    { "sr_data_bsdsocket", cl_hold_bsdsocket.guard },
    { "sr_data_area_sector", cl_hold_area_sector.guard },
    { "sr_data_game_world", cl_hold_game_world.guard },
    { "sr_data_tempmesh", cl_hold_tempmesh.guard },
    { "sr_data_track", cl_hold_track.guard },
    { "cl_data_bondhuman_client", cl_hold_bondhuman_client.guard },
    { "cl_data_auth_ticket", cl_hold_auth_ticket.guard },
    { "cl_data_auth_ticket_size", cl_hold_auth_ticket_size.guard },
    { "cl_data_auth_ticket_ready", cl_hold_auth_ticket_ready.guard },
    { "cl_data_play_ticket", cl_hold_play_ticket.guard },
    { "cl_data_play_ticket_on", cl_hold_play_ticket_on.guard },
    { "cl_data_voice_test_samples", cl_hold_voice_test_samples.guard },
    { "cl_data_image_partial", cl_hold_image_partial.guard },
    { "cl_data_zlib_default_length", cl_hold_zlib_default_length.guard },
    { "cl_data_zlib_default_distance", cl_hold_zlib_default_distance.guard },
    { "cl_data_image_error", cl_hold_image_error.guard },
    { "cl_data_fontnum", cl_hold_fontnum.guard },
    { "cl_data_fonts", cl_hold_fonts.guard },
    { "cl_data_font_bits", cl_hold_font_bits.guard },
    { "cl_data_cursor_bits", cl_hold_cursor_bits.guard },
    { "cl_data_image_loaders", cl_hold_image_loaders.guard },
    { "cl_data_image_loader_count", cl_hold_image_loader_count.guard },
    { "cl_data_image_hdr_gamma_inverse", cl_hold_image_hdr_gamma_inverse.guard },
    { "cl_data_image_hdr_scale_inverse", cl_hold_image_hdr_scale_inverse.guard },
    { "cl_data_image_ldr_gamma", cl_hold_image_ldr_gamma.guard },
    { "cl_data_image_ldr_scale", cl_hold_image_ldr_scale.guard },
    { "cl_data_wgl_loaders", cl_hold_wgl_loaders.guard },
    { "cl_data_image_name", cl_hold_image_name.guard },
    { "cl_data_sound_buffers", cl_hold_sound_buffers.guard },
    { "cl_data_audio_source_limit", cl_hold_audio_source_limit.guard },
    { "cl_data_source_buffer", cl_hold_source_buffer.guard },
    { "cl_data_track_verts", cl_hold_track_verts.guard },
    { "cl_data_building_click_wait", cl_hold_building_click_wait.guard },
    { "cl_data_head_face_quads", cl_hold_head_face_quads.guard },
    { "cl_data_key_event_count", cl_hold_key_event_count.guard },
    { "cl_data_key_event_next", cl_hold_key_event_next.guard },
    { "cl_data_key_events", cl_hold_key_events.guard },
    { "cl_data_message_delay", cl_hold_message_delay.guard },
    { "cl_data_num_joysticks", cl_hold_num_joysticks.guard },
    { "cl_data_joysticks", cl_hold_joysticks.guard },
    { "cl_data_joysticks_previous", cl_hold_joysticks_previous.guard },
    { "cl_data_joystick_handles", cl_hold_joystick_handles.guard },
    { "cl_data_text_back", cl_hold_text_back.guard },
    { "cl_data_text_voice_minimum", cl_hold_text_voice_minimum.guard },
    { "cl_data_text_push_to_talk", cl_hold_text_push_to_talk.guard },
    { "cl_data_text_push_key", cl_hold_text_push_key.guard },
    { "cl_data_text_voice_chat", cl_hold_text_voice_chat.guard },
    { "cl_data_text_speaking", cl_hold_text_speaking.guard },
    { "cl_data_text_level", cl_hold_text_level.guard },
    { "cl_data_text_int_format", cl_hold_text_int_format.guard },
    { "cl_data_text_bind_hint", cl_hold_text_bind_hint.guard },
    { "cl_data_text_empty", cl_hold_text_empty.guard },
    { "cl_data_text_speex_frame_size", cl_hold_text_speex_frame_size.guard },
    { "cl_data_text_bad_bmp", cl_hold_text_bad_bmp.guard },
    { "cl_data_text_png_unknown_chunk", cl_hold_text_png_unknown_chunk.guard },
    { "cl_data_atmo_constant", cl_hold_atmo_constant.guard },
    { "cl_data_atmo_wavelength", cl_hold_atmo_wavelength.guard },
    { "cl_data_sector_block", cl_hold_sector_block.guard },
    { "cl_data_app_ticket", cl_hold_app_ticket.guard },
    { "cl_data_modeltemp", cl_hold_modeltemp.guard },
    { "cl_data_model", cl_hold_model.guard },
    { "cl_data_texture", cl_hold_texture.guard },
    { "cl_data_image", cl_hold_image.guard },
    { "cl_data_landrender", cl_hold_landrender.guard },
    { "cl_data_bigendian", cl_hold_bigendian.guard },
    { "cl_data_landsector_vbo", cl_hold_landsector_vbo.guard },
    { "cl_data_vbo", cl_hold_vbo.guard },
    { "cl_data_landsector_lod", cl_hold_landsector_lod.guard },
    { "cl_data_fontlist", cl_hold_fontlist.guard },
    { "cl_data_sphere", cl_hold_sphere.guard },
    { "cl_data_texture_windowatlas", cl_hold_texture_windowatlas.guard },
    { "cl_data_texture_human", cl_hold_texture_human.guard },
    { "cl_data_bonevert", cl_hold_bonevert.guard },
    { "cl_data_cornervert", cl_hold_cornervert.guard },
    { "cl_data_draw_text", cl_hold_draw_text.guard },
    { "cl_data_draw_number", cl_hold_draw_number.guard },
    { "cl_data_glext", cl_hold_glext.guard },
    { "cl_data_programs", cl_hold_programs.guard },
    { "cl_data_shader_log", cl_hold_shader_log.guard },
    { "cl_data_info_log", cl_hold_info_log.guard },
    { "cl_data_render_width", cl_hold_render_width.guard },
    { "cl_data_render_height", cl_hold_render_height.guard },
    { "cl_data_open_file", cl_hold_open_file.guard },
    { "cl_data_light_position", cl_hold_light_position.guard },
    { "cl_data_world_position", cl_hold_world_position.guard },
    { "cl_data_viewport_width", cl_hold_viewport_width.guard },
    { "cl_data_viewport_height", cl_hold_viewport_height.guard },
    { "cl_data_track_vbo", cl_hold_track_vbo.guard },
    { "cl_data_track_vertex_floats", cl_hold_track_vertex_floats.guard },
    { "cl_data_track_vertex_count", cl_hold_track_vertex_count.guard },
    { "cl_data_frustum_planes", cl_hold_frustum_planes.guard },
    { "cl_data_shadow_corners", cl_hold_shadow_corners.guard },
    { "cl_data_shadow_count", cl_hold_shadow_count.guard },
    { "cl_data_shadow_list", cl_hold_shadow_list.guard },
    { "cl_data_chat_head", cl_hold_chat_head.guard },
    { "cl_data_sprite_models", cl_hold_sprite_models.guard },
    { "cl_data_shell_verts", cl_hold_shell_verts.guard },
    { "cl_data_debris_verts", cl_hold_debris_verts.guard },
    { "cl_data_score_count", cl_hold_score_count.guard },
    { "cl_data_score_entries", cl_hold_score_entries.guard },
    { "cl_data_display_text", cl_hold_display_text.guard },
    { "cl_data_light_ambient", cl_hold_light_ambient.guard },
    { "cl_data_interior_corners", cl_hold_interior_corners.guard },
    { "cl_data_sector_part_count", cl_hold_sector_part_count.guard },
    { "cl_data_sector_draw_counts", cl_hold_sector_draw_counts.guard },
    { "cl_data_sector_draw_offsets", cl_hold_sector_draw_offsets.guard },
    { "cl_data_sector_count", cl_hold_sector_count.guard },
    { "cl_data_sector_list", cl_hold_sector_list.guard },
    { "cl_data_sector_entries", cl_hold_sector_entries.guard },
    { "cl_data_interior_model_count", cl_hold_interior_model_count.guard },
    { "cl_data_sector_vertices", cl_hold_sector_vertices.guard },
    { "cl_data_sector_indices", cl_hold_sector_indices.guard },
    { "cl_data_sector_vbo", cl_hold_sector_vbo.guard },
    { "cl_data_sprites", cl_hold_sprites.guard },
    { "cl_data_sprite_decals", cl_hold_sprite_decals.guard },
    { "cl_data_glass_vertices", cl_hold_glass_vertices.guard },
    { "cl_data_tiremarks", cl_hold_tiremarks.guard },
    { "cl_data_selected_item", cl_hold_selected_item.guard },
    { "cl_data_sector_shadow_corners", cl_hold_sector_shadow_corners.guard },
    { "cl_data_sector_shadow_count", cl_hold_sector_shadow_count.guard },
    { "cl_data_sector_shadow_vertices", cl_hold_sector_shadow_vertices.guard },
    { "cl_data_sector_shadow_buffer_ready", cl_hold_sector_shadow_buffer_ready.guard },
    { "cl_data_sector_shadow_vbo", cl_hold_sector_shadow_vbo.guard },
    { "cl_data_frame_model_count", cl_hold_frame_model_count.guard },
    { "cl_data_frame_model_order", cl_hold_frame_model_order.guard },
    { "cl_data_frame_models", cl_hold_frame_models.guard },
    { "cl_data_video_modes", cl_hold_video_modes.guard },
    { "cl_data_video_mode_count", cl_hold_video_mode_count.guard },
    { "cl_data_screenshot_pixels", cl_hold_screenshot_pixels.guard },
    { "cl_data_blur_params_near", cl_hold_blur_params_near.guard },
    { "cl_data_viewport_x", cl_hold_viewport_x.guard },
    { "cl_data_viewport_y", cl_hold_viewport_y.guard },
    { "cl_data_light_diffuse", cl_hold_light_diffuse.guard },
    { "cl_data_sky_color", cl_hold_sky_color.guard },
    { "cl_data_view_near", cl_hold_view_near.guard },
    { "cl_data_view_far", cl_hold_view_far.guard },
    { "cl_data_view_fov", cl_hold_view_fov.guard },
    { "cl_data_view_side_right", cl_hold_view_side_right.guard },
    { "cl_data_view_side_left", cl_hold_view_side_left.guard },
    { "cl_data_window_quad_count", cl_hold_window_quad_count.guard },
    { "cl_data_window_quads", cl_hold_window_quads.guard },
    { "cl_data_framebuffers", cl_hold_framebuffers.guard },
    { "cl_data_framebuffer_index", cl_hold_framebuffer_index.guard },
    { "cl_data_input_mouse", cl_hold_input_mouse.guard },
    { "cl_data_input_mouse_previous", cl_hold_input_mouse_previous.guard },
    { "cl_data_input_cursor", cl_hold_input_cursor.guard },
    { "cl_data_input_cursor_previous", cl_hold_input_cursor_previous.guard },
    { "cl_data_input_fire_blocked", cl_hold_input_fire_blocked.guard },
    { "cl_data_input_chat", cl_hold_input_chat.guard },
    { "cl_data_input_chat_head", cl_hold_input_chat_head.guard },
    { "cl_data_input_chat_length", cl_hold_input_chat_length.guard },
    { "cl_data_input_chat_text", cl_hold_input_chat_text.guard },
    { "sr_data_kickedip", cl_hold_kickedip.guard },
    { "sr_data_numofkicked", cl_hold_numofkicked.guard },
    { "cl_data_input_debug_toggle", cl_hold_input_debug_toggle.guard },
    { "cl_data_input_label_chat", cl_hold_input_label_chat.guard },
    { "cl_data_input_label_pause", cl_hold_input_label_pause.guard },
    { "cl_data_input_label_return", cl_hold_input_label_return.guard },
    { "cl_data_input_label_options", cl_hold_input_label_options.guard },
    { "cl_data_input_label_voice", cl_hold_input_label_voice.guard },
    { "cl_data_input_label_exit", cl_hold_input_label_exit.guard },
    { "cl_data_input_label_return_connections", cl_hold_input_label_return_connections.guard },
    { "cl_data_input_label_empty_connection", cl_hold_input_label_empty_connection.guard },
    { "cl_data_input_label_human_axis", cl_hold_input_label_human_axis.guard },
    { "cl_data_input_label_timer_build", cl_hold_input_label_timer_build.guard },
    { "cl_data_input_label_timer_optimize", cl_hold_input_label_timer_optimize.guard },
    { "cl_data_input_label_timer_setup", cl_hold_input_label_timer_setup.guard },
    { "cl_data_input_label_timer_alloc", cl_hold_input_label_timer_alloc.guard },
    { "cl_data_input_label_timer_shadow", cl_hold_input_label_timer_shadow.guard },
    { "sr_data_objectpacket", cl_hold_objectpacket.guard },
    { "sr_data_itemtype", cl_hold_itemtype.guard },
    { "cl_data_menu_items", cl_hold_menu_items.guard },
    { "cl_data_menu_item_count", cl_hold_menu_item_count.guard },
    { "cl_data_menu_item_index", cl_hold_menu_item_index.guard },
    { "cl_data_textinput_start", cl_hold_textinput_start.guard },
    { "cl_data_textinput_end", cl_hold_textinput_end.guard },
    { "cl_data_textinput_text", cl_hold_textinput_text.guard },
    { "cl_data_textinput_shown", cl_hold_textinput_shown.guard },
    { "cl_data_menu_vertices", cl_hold_menu_vertices.guard },
    { "cl_data_menu_texcoords", cl_hold_menu_texcoords.guard },
    { "cl_data_menu_alpha", cl_hold_menu_alpha.guard },
    { "cl_data_menu_quad_count", cl_hold_menu_quad_count.guard },
    { "cl_data_menu_quads", cl_hold_menu_quads.guard },
    { "cl_data_wrap_line_ends", cl_hold_wrap_line_ends.guard },
    { "cl_data_client_account_id", cl_hold_client_account_id.guard },
    { "cl_data_textinput_overwrite", cl_hold_textinput_overwrite.guard },
    { "cl_data_wrap_line_count", cl_hold_wrap_line_count.guard },
    { "cl_data_screen_bits", cl_hold_screen_bits.guard },
    { "cl_data_screen_fullscreen", cl_hold_screen_fullscreen.guard },
    { "cl_data_joystick_names", cl_hold_joystick_names.guard },
    { "cl_data_join_servers", cl_hold_join_servers.guard },
    { "cl_data_join_server_count", cl_hold_join_server_count.guard },
    { "cl_data_join_addresses", cl_hold_join_addresses.guard },
    { "cl_data_join_address_count", cl_hold_join_address_count.guard },
    { "cl_data_join_master_first", cl_hold_join_master_first.guard },
    { "cl_data_join_columns", cl_hold_join_columns.guard },
    { "cl_data_vbo_next", cl_hold_vbo_next.guard },
    { "cl_data_framebuffer_ready", cl_hold_framebuffer_ready.guard },
    { "cl_data_vbo_spare_a", cl_hold_vbo_spare_a.guard },
    { "cl_data_vbo_spare_b", cl_hold_vbo_spare_b.guard },
    { "cl_data_vbo_spare_c", cl_hold_vbo_spare_c.guard },
    { "cl_data_version_marks", cl_hold_version_marks.guard },
    { "cl_data_audio_device", cl_hold_audio_device.guard },
    { "cl_data_audio_context", cl_hold_audio_context.guard },
    { "cl_data_audio_capture", cl_hold_audio_capture.guard },
    { "cl_data_audio_enabled", cl_hold_audio_enabled.guard },
    { "cl_data_audio_encoder", cl_hold_audio_encoder.guard },
    { "cl_data_audio_decoders", cl_hold_audio_decoders.guard },
    { "cl_data_audio_camera", cl_hold_audio_camera.guard },
    { "cl_data_audio_sounds", cl_hold_audio_sounds.guard },
    { "cl_data_audio_sound_count", cl_hold_audio_sound_count.guard },
    { "cl_data_audio_objects", cl_hold_audio_objects.guard },
    { "cl_data_audio_object_count", cl_hold_audio_object_count.guard },
    { "cl_data_audio_play_buffer", cl_hold_audio_play_buffer.guard },
    { "cl_data_audio_input_blocked", cl_hold_audio_input_blocked.guard },
    { "cl_data_audio_voice_mode", cl_hold_audio_voice_mode.guard },
    { "cl_data_audio_menu_back", cl_hold_audio_menu_back.guard },
    { "cl_data_audio_window", cl_hold_audio_window.guard },
    { "cl_data_audio_key_names", cl_hold_audio_key_names.guard },
    { "cl_data_keyboard", cl_hold_keyboard.guard },
    { "cl_data_prevkeyboard", cl_hold_prevkeyboard.guard },
    { "cl_data_chatmessage", cl_hold_chatmessage.guard },
    { "cl_data_render_aspect", cl_hold_render_aspect.guard },
    { "cl_data_render_depth_bits", cl_hold_render_depth_bits.guard },
    { "cl_data_render_stencil_bits", cl_hold_render_stencil_bits.guard },
    { "cl_data_gl_context", cl_hold_gl_context.guard },
    { "cl_data_steam_active", cl_hold_steam_active.guard },
    { "cl_data_player_marks", cl_hold_player_marks.guard },
    { "cl_data_building_editor", cl_hold_building_editor.guard },
    { "cl_data_building_view", cl_hold_building_view.guard },
    { "cl_data_editor_tile_count", cl_hold_editor_tile_count.guard },
    { "cl_data_editor_tiles", cl_hold_editor_tiles.guard },
    { "cl_data_edit_vertices", cl_hold_edit_vertices.guard },
    { "cl_data_edit_grabbed", cl_hold_edit_grabbed.guard },
    { "cl_data_edit_offsets", cl_hold_edit_offsets.guard },
    { "cl_data_edit_mouse_before", cl_hold_edit_mouse_before.guard },
    { "cl_data_game_block_count", cl_hold_game_block_count.guard },
    { "cl_data_game_blocks", cl_hold_game_blocks.guard },
    { "cl_data_game_texture_count", cl_hold_game_texture_count.guard },
    { "cl_data_game_textures", cl_hold_game_textures.guard },
};

/* Return the name of an object whose guard changed, and mend the guard. Return 0 when all are whole. */
const char *cl_guards_scan(unsigned *first, unsigned *last)
{
    unsigned i, k;

    for (i = 0; i < sizeof cl_guards / sizeof cl_guards[0]; i++)
        for (k = 0; k < CL_GUARD_SIZE; k++)
            if (cl_guards[i].guard[k] != 0xa5) {
                unsigned end = CL_GUARD_SIZE;

                while (end > k && cl_guards[i].guard[end - 1] == 0xa5)
                    end--;
                *first = k;
                *last = end - 1;
                memset(cl_guards[i].guard, 0xa5, CL_GUARD_SIZE);
                return cl_guards[i].name;
            }
            
    return 0;
}
#endif

/* Give the objects their start values. Call it once, before the game starts. */
void cl_storage_init(void)
{
#ifdef SR_STORAGE_GUARDS
    unsigned i;

    for (i = 0; i < sizeof cl_guards / sizeof cl_guards[0]; i++)
        memset(cl_guards[i].guard, 0xa5, CL_GUARD_SIZE);
#endif
    {
        static const unsigned char value[] = { 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_hack_modulusopt, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f };
        memcpy(sr_data_cubevertex, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubenormal, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubetangent, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f };
        memcpy(sr_data_cubebinormal, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubetexcoord, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubequad, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubetri, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cubeline, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_octahedron, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x05, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_octahedronindex, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x80, 0x3f };
        memcpy(sr_data_debug_cubevertex, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x18, 0x00, 0x00, 0x00 };
        memcpy(&sr_data_currentversion, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x10, 0x3f, 0x00, 0x00, 0x40, 0x3e, 0x00, 0x00, 0x40, 0x3e, 0x00, 0x00, 0xc0, 0x3e, 0x00, 0x00, 0x10, 0x3f, 0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x10, 0x3f, 0x00, 0x00, 0x40, 0x3e, 0x00, 0x00, 0x40, 0x3e, 0x00, 0x00, 0x60, 0x3f, 0x00, 0x00, 0x60, 0x3f, 0x00, 0x00, 0x60, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x80, 0x3e };
        memcpy(sr_data_teamcolor, value, sizeof value);
    }
    sr_data_teamname[0] = "Goldmen Inc";
    sr_data_teamname[1] = "Monsota";
    sr_data_teamname[2] = "OXS International";
    sr_data_teamname[3] = "Nexaco";
    sr_data_teamname[4] = "Pentacom";
    sr_data_teamname[5] = "team 6";
    sr_data_teamname[6] = "team 7";
    sr_data_teamname[7] = "Civilian";
    {
        static const unsigned char value[] = { 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_dollarbillvalue, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(sr_data_cslogo, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0xa9, 0xa8, 0x28, 0x3e, 0x89, 0x88, 0x88, 0x3e, 0xb5, 0xb4, 0xb4, 0x3e };
        memcpy(sr_data_menucolor, value, sizeof value);
    }
    sr_data_csmstr = "CSMo";
    {
        static const unsigned char value[] = { 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00 };
        memcpy(sr_data_tracktrailmodelquad, value, sizeof value);
    }
    sr_data_carcolorname[0] = "Black";
    sr_data_carcolorname[1] = "Red";
    sr_data_carcolorname[2] = "Blue";
    sr_data_carcolorname[3] = "Silver";
    sr_data_carcolorname[4] = "White";
    sr_data_carcolorname[5] = "Gold";
    {
        static const unsigned char value[] = { 0x71, 0x02, 0x00, 0x00 };
        memcpy(&sr_data_mti, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        memcpy(&sr_data_xaxis, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00 };
        memcpy(&sr_data_yaxis, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f };
        memcpy(&sr_data_zaxis, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x37, 0x44, 0x46, 0x50 };
        memcpy(&sr_data_socketid, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x6e, 0x69, 0x67, 0x67, 0x65, 0x72, 0x00 };
        memcpy(sr_data_filtered_word, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0xdf, 0xb0, 0x08, 0x99 };
        memcpy(sr_data_mag01, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x18, 0x18, 0x18, 0xf8, 0xf8, 0x18, 0x18, 0x18, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x18, 0x18, 0x18, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x1f, 0x1f, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x18, 0x18, 0x18, 0xc0, 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x07, 0x03, 0x03, 0x07, 0x0e, 0x1c, 0x38, 0x70, 0xe0, 0xc0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x38, 0x38, 0xee, 0xee, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xf8, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18, 0x00, 0x00, 0x3c, 0x7e, 0x7e, 0x7e, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0xff, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x18, 0x18, 0x18, 0xf8, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x18, 0x0c, 0x7e, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x18, 0x30, 0x7e, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0xff, 0x66, 0x66, 0xff, 0x66, 0x00, 0x18, 0x7c, 0x06, 0x3c, 0x60, 0x3e, 0x18, 0x00, 0x00, 0x66, 0x36, 0x18, 0x0c, 0x66, 0x62, 0x00, 0x38, 0x6c, 0x38, 0x1c, 0xf6, 0x66, 0xdc, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x38, 0x18, 0x18, 0x38, 0x70, 0x00, 0x00, 0x0e, 0x1c, 0x18, 0x18, 0x1c, 0x0e, 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00, 0x00, 0x3c, 0x66, 0x76, 0x6e, 0x66, 0x3c, 0x00, 0x00, 0x18, 0x1c, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x3c, 0x66, 0x30, 0x18, 0x0c, 0x7e, 0x00, 0x00, 0x7e, 0x30, 0x18, 0x30, 0x66, 0x3c, 0x00, 0x00, 0x30, 0x38, 0x3c, 0x36, 0x7e, 0x30, 0x00, 0x00, 0x7e, 0x06, 0x3e, 0x60, 0x66, 0x3c, 0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x7e, 0x60, 0x30, 0x18, 0x0c, 0x0c, 0x00, 0x00, 0x3c, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x3c, 0x66, 0x7c, 0x60, 0x30, 0x1c, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x0c, 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x3c, 0x66, 0x30, 0x18, 0x00, 0x18, 0x00, 0x00, 0x3c, 0x66, 0x76, 0x76, 0x06, 0x7c, 0x00, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x00, 0x00, 0x3e, 0x66, 0x3e, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x3c, 0x66, 0x06, 0x06, 0x66, 0x3c, 0x00, 0x00, 0x1e, 0x36, 0x66, 0x66, 0x36, 0x1e, 0x00, 0x00, 0x7e, 0x06, 0x3e, 0x06, 0x06, 0x7e, 0x00, 0x00, 0x7e, 0x06, 0x3e, 0x06, 0x06, 0x06, 0x00, 0x00, 0x7c, 0x06, 0x06, 0x76, 0x66, 0x7c, 0x00, 0x00, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x66, 0x3c, 0x00, 0x00, 0x66, 0x36, 0x1e, 0x1e, 0x36, 0x66, 0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x7e, 0x00, 0x00, 0xc6, 0xee, 0xfe, 0xd6, 0xc6, 0xc6, 0x00, 0x00, 0x66, 0x6e, 0x7e, 0x7e, 0x76, 0x66, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x36, 0x6c, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x36, 0x66, 0x00, 0x00, 0x3c, 0x06, 0x3c, 0x60, 0x60, 0x3c, 0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00, 0x00, 0x66, 0x66, 0x3c, 0x3c, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x00, 0x00, 0x7e, 0x30, 0x18, 0x0c, 0x06, 0x7e, 0x00, 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00, 0x00, 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x3c, 0x60, 0x7c, 0x66, 0x7c, 0x00, 0x00, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, 0x3c, 0x06, 0x06, 0x06, 0x3c, 0x00, 0x00, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x7e, 0x06, 0x3c, 0x00, 0x00, 0x70, 0x18, 0x7c, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x3e, 0x00, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x00, 0x00, 0x18, 0x00, 0x1c, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x60, 0x00, 0x60, 0x60, 0x60, 0x60, 0x3c, 0x00, 0x06, 0x06, 0x36, 0x1e, 0x36, 0x66, 0x00, 0x00, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x66, 0xfe, 0xfe, 0xd6, 0xc6, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x00, 0x00, 0x3e, 0x66, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x7c, 0x06, 0x3c, 0x60, 0x3e, 0x00, 0x00, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, 0xc6, 0xd6, 0xfe, 0x7c, 0x6c, 0x00, 0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x7c, 0x30, 0x1e, 0x00, 0x00, 0x7e, 0x30, 0x18, 0x0c, 0x7e, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0x7e, 0x18, 0x3c, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x77, 0x77, 0x77, 0x00, 0x77, 0x77, 0x77, 0x00, 0x10, 0x3c, 0x7e, 0x3f, 0x1f, 0x3f, 0x7e, 0x3c, 0x08, 0x18, 0x38, 0x78, 0x38, 0x18, 0x08, 0x00 };
        memcpy(cl_data_font_bits, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x3f, 0x1f, 0x0f, 0x1f, 0x3b, 0x71, 0x60, 0x00 };
        memcpy(cl_data_cursor_bits, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x2e, 0xba, 0xe8, 0x3e };
        memcpy(&cl_data_image_hdr_gamma_inverse, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x80, 0x3f };
        memcpy(&cl_data_image_hdr_scale_inverse, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0xcd, 0xcc, 0x0c, 0x40 };
        memcpy(&cl_data_image_ldr_gamma, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x80, 0x3f };
        memcpy(&cl_data_image_ldr_scale, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x42, 0x61, 0x63, 0x6b, 0x00 };
        memcpy(cl_data_text_back, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x56, 0x6f, 0x69, 0x63, 0x65, 0x20, 0x4d, 0x69, 0x6e, 0x69, 0x6d, 0x75, 0x6d, 0x00 };
        memcpy(cl_data_text_voice_minimum, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x55, 0x73, 0x65, 0x20, 0x70, 0x75, 0x73, 0x68, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x61, 0x6c, 0x6b, 0x00 };
        memcpy(cl_data_text_push_to_talk, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x55, 0x73, 0x65, 0x20, 0x70, 0x75, 0x73, 0x68, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x61, 0x6c, 0x6b, 0x00 };
        memcpy(cl_data_text_push_key, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x56, 0x6f, 0x69, 0x63, 0x65, 0x20, 0x43, 0x68, 0x61, 0x74, 0x00 };
        memcpy(cl_data_text_voice_chat, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x4f, 0x00 };
        memcpy(cl_data_text_speaking, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x56, 0x00 };
        memcpy(cl_data_text_level, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x2f, 0x69, 0x00 };
        memcpy(cl_data_text_int_format, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x6f, 0x72, 0x20, 0x6d, 0x6f, 0x75, 0x73, 0x65, 0x20, 0x62, 0x75, 0x74, 0x74, 0x6f, 0x6e, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x76, 0x6f, 0x69, 0x63, 0x65, 0x20, 0x70, 0x75, 0x73, 0x68, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x61, 0x6c, 0x6b, 0x00 };
        memcpy(cl_data_text_bind_hint, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x73, 0x70, 0x65, 0x65, 0x78, 0x66, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x69, 0x7a, 0x65, 0x00 };
        memcpy(cl_data_text_speex_frame_size, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x62, 0x61, 0x64, 0x20, 0x42, 0x4d, 0x50, 0x00 };
        memcpy(cl_data_text_bad_bmp, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x58, 0x58, 0x58, 0x58, 0x20, 0x63, 0x68, 0x75, 0x6e, 0x6b, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x6b, 0x6e, 0x6f, 0x77, 0x6e, 0x00 };
        memcpy(cl_data_text_png_unknown_chunk, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x74, 0xbb, 0x4d, 0x3d, 0x17, 0xb7, 0xd1, 0x38, 0xcd, 0xcc, 0xcc, 0x40, 0x66, 0x46, 0x1f, 0x44 };
        memcpy(cl_data_atmo_constant, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x20, 0x3f, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x80, 0x3f };
        memcpy(cl_data_atmo_wavelength, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x81, 0x04, 0x35, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x81, 0x04, 0x35, 0x3f, 0x81, 0x04, 0x35, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x81, 0x04, 0x35, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x81, 0x04, 0x35, 0xbf, 0x81, 0x04, 0x35, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x81, 0x04, 0x35, 0x3f, 0x81, 0x04, 0x35, 0x3f };
        memcpy(cl_data_frustum_planes, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x45, 0x78, 0x69, 0x74, 0x00 };
        memcpy(cl_data_input_label_pause, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x52, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x20, 0x74, 0x6f, 0x20, 0x47, 0x61, 0x6d, 0x65, 0x00 };
        memcpy(cl_data_input_label_return, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x4f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x00 };
        memcpy(cl_data_input_label_options, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x56, 0x6f, 0x69, 0x63, 0x65, 0x20, 0x4f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x00 };
        memcpy(cl_data_input_label_voice, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x45, 0x20, 0x74, 0x6f, 0x20, 0x65, 0x78, 0x69, 0x74, 0x00 };
        memcpy(cl_data_input_label_exit, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x52, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x20, 0x74, 0x6f, 0x20, 0x47, 0x61, 0x6d, 0x65, 0x00 };
        memcpy(cl_data_input_label_return_connections, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x41, 0x6e, 0x6f, 0x6e, 0x00 };
        memcpy(cl_data_input_label_empty_connection, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x68, 0x75, 0x6d, 0x61, 0x6e, 0x61, 0x78, 0x69, 0x73, 0x00 };
        memcpy(cl_data_input_label_human_axis, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x62, 0x75, 0x69, 0x6c, 0x64, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6f, 0x72, 0x00 };
        memcpy(cl_data_input_label_timer_build, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x6f, 0x70, 0x74, 0x69, 0x6d, 0x69, 0x7a, 0x65, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6f, 0x72, 0x00 };
        memcpy(cl_data_input_label_timer_optimize, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x73, 0x65, 0x74, 0x75, 0x70, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6f, 0x72, 0x00 };
        memcpy(cl_data_input_label_timer_setup, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x61, 0x6c, 0x6c, 0x6f, 0x63, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6f, 0x72, 0x00 };
        memcpy(cl_data_input_label_timer_alloc, value, sizeof value);
    }
    {
        static const unsigned char value[] = { 0x73, 0x68, 0x61, 0x64, 0x6f, 0x77, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x69, 0x6f, 0x72, 0x00 };
        memcpy(cl_data_input_label_timer_shadow, value, sizeof value);
    }
}

#endif
