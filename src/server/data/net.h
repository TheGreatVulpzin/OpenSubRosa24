/* Declarations of the data of packets, connections and the voice chat. Include sr_storage.h, not this file. */
#ifndef SV_DATA_NET_H
#define SV_DATA_NET_H
extern char sr_data_filtered_word[7];
SR_STATIC_ASSERT(sizeof(sr_data_filtered_word) == 7, "storage filtered_word");
extern int sr_data_currentversion;
SR_STATIC_ASSERT(sizeof(sr_data_currentversion) == 4, "storage currentversion");
extern char sr_data_message[64];
SR_STATIC_ASSERT(sizeof(sr_data_message) == 64, "storage message");
extern int sr_data_numofconnections;
SR_STATIC_ASSERT(sizeof(sr_data_numofconnections) == 4, "storage numofconnections");
extern int sr_data_view;
SR_STATIC_ASSERT(sizeof(sr_data_view) == 4, "storage view");
extern Speex sr_data_speex;
SR_STATIC_ASSERT(sizeof(sr_data_speex) == 9637888, "storage speex");
extern Connection sr_data_connection[256];
SR_STATIC_ASSERT(sizeof(sr_data_connection) == 15822848, "storage connection");
extern Market sr_data_market;
SR_STATIC_ASSERT(sizeof(sr_data_market) == 516, "storage market");
extern MasterServer sr_data_masterserver;
SR_STATIC_ASSERT(sizeof(sr_data_masterserver) == 520, "storage masterserver");
#endif
