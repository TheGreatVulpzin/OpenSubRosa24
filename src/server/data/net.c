/* Data of packets, connections and the voice chat. Edit this file like any other source. */
#include "sr_storage.h"

char sr_data_filtered_word[7] = {
    110, 105, 103, 103, 101, 114, 0
};
int sr_data_currentversion = 24;
char sr_data_message[64];
int sr_data_numofconnections;
int sr_data_view;
Speex sr_data_speex;
Connection sr_data_connection[256];
Market sr_data_market;
MasterServer sr_data_masterserver;
