/* Data that belongs to no system file. It has no guard in the guard build. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

char sr_unplaced_gamedir[9];
float sr_unplaced_lightwavelength[3];
float sr_unplaced_xaxis2d[2];
float sr_unplaced_yaxis2d[2];
BondHuman sr_unplaced_bondhuman[512];
int sr_unplaced_startup_counter;
SrPacketWorkspace sr_unplaced_packet_workspace;
int sr_client_end_time;
int sr_client_gametype;

#endif
