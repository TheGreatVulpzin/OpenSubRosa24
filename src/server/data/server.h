/* Declarations of the data of the server loop, the players and the accounts. Include sr_storage.h, not this file. */
#ifndef SV_DATA_SERVER_H
#define SV_DATA_SERVER_H
extern int sr_data_dedicated;
SR_STATIC_ASSERT(sizeof(sr_data_dedicated) == 4, "storage dedicated");
extern uint8_t sr_data_wsadata[400];
SR_STATIC_ASSERT(sizeof(sr_data_wsadata) == 400, "storage wsadata");
extern int sr_data_startup_counter;
SR_STATIC_ASSERT(sizeof(sr_data_startup_counter) == 4, "storage startup_counter");
#endif
