/* Declarations of data that belongs to no system file. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_UNPLACED_H
#define CL_DATA_UNPLACED_H
extern char sr_unplaced_gamedir[9];
extern float sr_unplaced_lightwavelength[3];
extern float sr_unplaced_xaxis2d[2];
extern float sr_unplaced_yaxis2d[2];
extern BondHuman sr_unplaced_bondhuman[512];
extern int sr_unplaced_startup_counter;
extern SrPacketWorkspace sr_unplaced_packet_workspace;
extern int sr_client_end_time;
extern int sr_client_gametype;
#define sr_data_gamedir sr_unplaced_gamedir
#define sr_data_lightwavelength sr_unplaced_lightwavelength
#define sr_data_xaxis2d sr_unplaced_xaxis2d
#define sr_data_yaxis2d sr_unplaced_yaxis2d
#define sr_data_bondhuman sr_unplaced_bondhuman
#define sr_data_startup_counter sr_unplaced_startup_counter
#define sr_data_packet_workspace sr_unplaced_packet_workspace
#endif
