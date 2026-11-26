/* Declarations of the data of the tables that the shared helper code reads. Include sr_storage.h, not this file. */
#ifndef SV_DATA_COMMON_H
#define SV_DATA_COMMON_H
extern int sr_data_mti;
SR_STATIC_ASSERT(sizeof(sr_data_mti) == 4, "storage mti");
extern uint32_t sr_data_mt[624];
SR_STATIC_ASSERT(sizeof(sr_data_mt) == 2496, "storage mt");
extern int sr_data_tzset_done;
SR_STATIC_ASSERT(sizeof(sr_data_tzset_done) == 4, "storage tzset_done");
extern int sr_data_debugit;
SR_STATIC_ASSERT(sizeof(sr_data_debugit) == 4, "storage debugit");
extern Debug sr_data_debug;
SR_STATIC_ASSERT(sizeof(sr_data_debug) == 336840, "storage debug");
extern uint32_t sr_data_random_counter;
SR_STATIC_ASSERT(sizeof(sr_data_random_counter) == 4, "storage random_counter");
extern void * sr_data_slogfp;
SR_STATIC_ASSERT(sizeof(sr_data_slogfp) == 4, "storage slogfp");
#endif
