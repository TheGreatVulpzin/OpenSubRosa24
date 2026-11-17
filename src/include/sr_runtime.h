/* Shared game storage access. */
#ifndef SR_RUNTIME_H
#define SR_RUNTIME_H

#include <stdint.h>

#ifdef SR_TEST_MEMORY
#include "storage_adapter.h"
#else
#include "sr_storage.h"
#endif
#include "sr_limits.h"
#include "sr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void sv_log(const char *fmt, ...);
void sv_profile_report(void);

#define sr_server       sr_data_server.active
#define sr_game         sr_data_game
/* The game tick. A storage adapter can place it outside Game. */
#ifndef sr_game_ticks
#define sr_game_ticks   sr_game.ticks
#endif
/* The time of the end phase. The client record has no such field: a storage adapter places it. */
#ifndef sr_game_end_time
#define sr_game_end_time sr_game.end_time
#endif
/* The client can mark a point in the world with a debug cross: define sr_debug_cross(center, size). */
/* The game type of an option record. The client record has no such field: a storage adapter places it. */
#ifndef sr_option_gametype
#define sr_option_gametype(o) ((o)->gametype)
#endif
/* The handle of the file that the text parser reads. A storage adapter can place it outside Parser. */
#ifndef sr_parser_file
#define sr_parser_file  sr_data_parser.file
#endif
/* A storage adapter selects the playback channel layout. */
#ifndef sr_voice_channel_type
#define sr_voice_channel_type VoiceChannel
#endif
#define sr_events       sr_data_event
#define sr_numofevents  sr_data_numofevents
#define sr_players      sr_data_player
#define sr_items        sr_data_item
#define sr_itemtypes    sr_data_itemtype
#define sr_humans       sr_data_human
#define sr_objects      sr_data_object
#define sr_bullets      sr_data_bullet
#define sr_numofbullets sr_data_numofbullets
#define sr_stats        sr_data_stats
#define sr_particles    sr_data_particle
#define sr_numofparticles sr_data_area_sector.num_particles
#define sr_bonds        sr_data_bond
#define sr_numofbonds   sr_data_numofbonds
#define sr_bondopt      sr_data_bondopt
#define sr_bondparticles sr_data_bondparticle
#define sr_bondhumans   sr_data_bondhuman
#define sr_bsdsocket    sr_data_bsdsocket
#define sr_socketid     sr_data_socketid /* "7DFP": the first 4 bytes of each packet */

#ifdef __cplusplus
}
#endif

#endif /* SR_RUNTIME_H */
