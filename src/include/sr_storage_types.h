/* Storage records shared by the server and its verification adapter. */
#ifndef SR_STORAGE_TYPES_H
#define SR_STORAGE_TYPES_H

#include "sr_types.h"
#include "sr_limits.h"

typedef size_t (__cdecl *SrFileTransfer)(void *, size_t, size_t, void *);

typedef struct SrAccountStore {
    uint32_t key;
    int count;
    Account records[65536];
} SrAccountStore;

typedef struct SrLandscape {
    vec3 origin;
    float heights[2049 * 2049];
} SrLandscape;

typedef struct SrSurface {
    vec3 points[16];
    float uv[32];
} SrSurface;

typedef struct SrSectorStore {
    int unused_0;
    int client_game_id;
    uint8_t reserved[0x1c];
    int client_connected;
    int client_timeout;
    int client_event_total;
    int num_particles;
    uint8_t reserved_end[0xc];
    BlockSector records[8192];
} SrSectorStore;

typedef struct SrInteriorModel {
    int sector;
    uint32_t mesh[0x2a2c / 4];
} SrInteriorModel;

typedef struct SrInteriorStore {
    int count;
    SrInteriorModel records[4096];
} SrInteriorStore;

typedef struct SrClientAccountStore {
    int count;
    int records[65536][4];
} SrClientAccountStore;

typedef struct SrMaterialStore {
    int count;
    MaterialRec records[1024];
} SrMaterialStore;

typedef struct BulletType {
    float mass;
    float drag;
    float caliber;
    float length;
    float unk_10;
    float unk_14;
} BulletType;

/* Long kick lists share their words with packet records and item types. */
typedef struct SrPacketWorkspace {
    union {
        uint32_t kickedip[SR_MAX_KICKEDIP];
        struct {
            uint32_t prefix[2064];
            ObjectPacket objectpacket;
            uint8_t gap[12];
            ItemType itemtype[64];
        };
    };
    int numofkicked;
} SrPacketWorkspace;

SR_STATIC_ASSERT(offsetof(SrAccountStore, records) == 8, "account header");
SR_STATIC_ASSERT(offsetof(SrSectorStore, records) == 0x40, "sector header");
SR_STATIC_ASSERT(sizeof(SrInteriorModel) == 0x2a30, "interior model");
SR_STATIC_ASSERT(sizeof(SrSurface) == 0x140, "surface");
SR_STATIC_ASSERT(sizeof(BulletType) == 0x18, "bullet type");
SR_STATIC_ASSERT(offsetof(SrPacketWorkspace, objectpacket) == 0x2040, "packet workspace");
SR_STATIC_ASSERT(offsetof(SrPacketWorkspace, itemtype) == 0xee60, "item type workspace");
SR_STATIC_ASSERT(offsetof(SrPacketWorkspace, numofkicked) == 0x28660, "kick list capacity");
SR_STATIC_ASSERT(sizeof(SrPacketWorkspace) == 0x28664, "packet workspace size");

#endif
