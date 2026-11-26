/* The data of the server. One file for each system in src/server/data. */
#ifndef SR_STORAGE_H
#define SR_STORAGE_H
#include "sr_storage_types.h"


#define sr_data_kickedip sr_data_packet_workspace.kickedip
#define sr_data_objectpacket sr_data_packet_workspace.objectpacket
#define sr_data_itemtype sr_data_packet_workspace.itemtype
#define sr_data_numofkicked sr_data_packet_workspace.numofkicked

/* The data of each system is in src/server/data. */
#include "../server/data/common.h"
#include "../server/data/game.h"
#include "../server/data/net.h"
#include "../server/data/server.h"

#endif
