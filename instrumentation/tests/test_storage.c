/* Compare storage defaults and deterministic game operations in both memory modes. */
#include <stdint.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_runtime.h"
#include "sr_server.h"
#include "platform.h"
#include "bonds.h"
#include "buildings.h"
#include "bullets.h"
#include "events.h"
#include "humans.h"
#include "items.h"
#include "object_types.h"
#include "objects.h"
#include "particles.h"
#include "packet.h"
#include "player.h"
#include "random.h"
#include "sr_math.h"
#include "sr_os.h"

extern uintptr_t sr_base;
int sv_record_memory_init(void);

void *sr_import(const char *dll, const char *name) { return plat_lookup(dll, name, 0); }
void *sr_import_ordinal(const char *dll, unsigned ordinal) { return plat_lookup(dll, NULL, ordinal); }
void sv_log(const char *fmt, ...) { (void)fmt; }

typedef struct Region {
    unsigned rva;
    const void *data;
    size_t size;
    int strings;
    const char *name;
} Region;

#define SR_STORAGE_REGION(rva, data, size, strings, name) + 1
enum { REGION_COUNT = 0
#include "storage_regions.inc"
};
#undef SR_STORAGE_REGION
static Region regions[REGION_COUNT];

static void regions_init(void)
{
    unsigned index = 0;
#define SR_STORAGE_REGION(rva, data, size, strings, name) regions[index++] = (Region){rva, data, size, strings, name};
#include "storage_regions.inc"
#undef SR_STORAGE_REGION
}

static int defaults(void)
{
    size_t i;
    for (i = 0; i < sizeof regions / sizeof regions[0]; ++i) {
        const Region *r = &regions[i];
        const void *expected = (const void *)(sr_base + r->rva);
        if (r->strings) {
            size_t j;
            for (j = 0; j < r->size / sizeof(char *); ++j)
                if (strcmp(((const char *const *)r->data)[j], ((const char *const *)expected)[j])) {
                    fprintf(stderr, "default string differs: %s[%u]\n", r->name, (unsigned)j);
                    return 0;
                }
        } else if (memcmp(r->data, expected, r->size)) {
            fprintf(stderr, "default bytes differ: %s\n", r->name);
            return 0;
        }
    }
    printf("initializers: %u storage objects match\n", (unsigned)REGION_COUNT);
    return 1;
}

static uint64_t hash(uint64_t h, const void *p, size_t size)
{
    const uint8_t *bytes = p;
    while (size--) {
        h ^= *bytes++;
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static void digest(const char *stage)
{
    size_t i;
    for (i = 0; i < sizeof regions / sizeof regions[0]; ++i) {
        const Region *r = &regions[i];
        uint64_t h = UINT64_C(14695981039346656037);
        if (r->strings) {
            size_t j;
            for (j = 0; j < r->size / sizeof(char *); ++j) {
                const char *s = ((const char *const *)r->data)[j];
                h = hash(h, s, strlen(s) + 1);
            }
        } else {
            h = hash(h, r->data, r->size);
        }
        printf("%s %s %016llx\n", stage, r->name, (unsigned long long)h);
    }
}

static int boundaries(void)
{
    uint8_t dst[8], expected[8];
    uint32_t value;
    static const unsigned widths[] = {8, 16, 6, 12};
    size_t k;
    int i;
    FILE *file;
    memset(dst, 0xa5, sizeof dst);
    memcpy(expected, dst, sizeof dst);
    sr_bsdsocket.recv_size = 8;
    sr_bsdsocket.recv_pos = 4;
    sr_bsdsocket.recv_bit = 0;
    if (cs_pread(dst, 2, 3) != 0 || memcmp(dst, expected, sizeof dst) ||
        sr_bsdsocket.recv_pos != 65536 || sr_bsdsocket.recv_bit != 0)
        return 0;
    for (k = 0; k < sizeof widths / sizeof widths[0]; ++k) {
        sr_bsdsocket.recv_size = -1;
        sr_bsdsocket.recv_pos = 81;
        sr_bsdsocket.recv_bit = 1;
        value = 0x12345678;
        if (pbitreadnego(&value, widths[k]) != 0 || value != 0x12345678 ||
            sr_bsdsocket.recv_size != -1 || sr_bsdsocket.recv_pos != 81 || sr_bsdsocket.recv_bit != 1)
            return 0;
    }
    sr_data_kickedip[offsetof(SrPacketWorkspace, objectpacket) / 4] = 17;
    if (sr_data_objectpacket.num_humans != 17)
        return 0;
    sr_data_itemtype[0].mass = 1.0f;
    if (sr_data_kickedip[(offsetof(SrPacketWorkspace, itemtype) + offsetof(ItemType, mass)) / 4] != 0x3f800000)
        return 0;
    sr_data_numofkicked = SR_MAX_KICKEDIP;
    sr_data_kickedip[SR_MAX_KICKEDIP - 1] = 0x0a010203;
    if (sr_data_numofkicked != SR_MAX_KICKEDIP || sr_data_kickedip[SR_MAX_KICKEDIP - 1] != 0x0a010203)
        return 0;
    file = fopen("readme.txt", "wbx");
    if (!file)
        return 0;
    for (i = 0; i < 1022; ++i)
        fputs("News\r\n", file);
    fclose(file);
    sr_data_accounts.count = 10;
    for (i = 0; i < 10; ++i) {
        sprintf(sr_data_accounts.records[i].name, "User%d", i);
        sr_data_accounts.records[i].money = i;
    }
    sr_data_newspaper.unk_40004 = 0x12345678;
    newspaper_load();
    remove("readme.txt");
    if (sr_data_newspaper.num_lines != 1024 || sr_data_newspaper.unk_40004 != 0x12345678)
        return 0;
    puts("boundaries: 9 cases passed");
    return 1;
}

static size_t read_file(void *p, size_t size, size_t count, void *file)
{
    return fread(p, size, count, file);
}

static size_t write_file(void *p, size_t size, size_t count, void *file)
{
    return fwrite(p, size, count, file);
}

static int client_accounts(void)
{
    int i, j;
    int capacity = (int)(sizeof sr_data_clientaccounts.records / sizeof sr_data_clientaccounts.records[0]);
    int header[2];
    FILE *file;
    sr_data_file_read = read_file;
    sr_data_file_write = write_file;
    strcpy(sr_data_sysinfo.start_dir, ".");
    strcpy(sr_data_sysinfo.config_dir, ".");
    sr_data_clientaccounts.count = capacity;
    for (i = 0; i < capacity; ++i)
        for (j = 0; j < 4; ++j)
            sr_data_clientaccounts.records[i][j] = 0x12340000 ^ (i << 4) ^ j;
    saveaccountsclient();
    file = fopen("client.srk", "rb");
    if (!file)
        return 0;
    if (fread(header, sizeof header, 1, file) != 1 || header[0] != 1 || header[1] != capacity ||
        fseek(file, 0, SEEK_END) != 0 || ftell(file) != 8 + 16 * capacity) {
        fclose(file);
        return 0;
    }
    fclose(file);
    memset(&sr_data_clientaccounts, 0, sizeof sr_data_clientaccounts);
    loadaccountsclient();
    remove("client.srk");
    if (sr_data_clientaccounts.count != capacity)
        return 0;
    for (i = 0; i < capacity; ++i)
        for (j = 0; j < 4; ++j)
            if (sr_data_clientaccounts.records[i][j] != (0x12340000 ^ (i << 4) ^ j))
                return 0;
    puts("client accounts: 65536 records saved and loaded");
    return 1;
}

int main(void)
{
    static const uint32_t random_values[] = {3499211612u, 581869302u, 3890346734u, 3586334585u, 545404204u};
    vec3 pos = {10, 20, 30};
    vec3 rot[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int i, human;
    uint32_t item;
    unsigned cw;
    if (!sv_record_memory_init())
        return 1;
    regions_init();
    if (!defaults())
        return 1;
    _controlfp_s(&cw, _PC_53, _MCW_PC);
    x87_init();
    init_genrand(5489);
    for (i = 0; i < 5; ++i)
        if (genrand_int32() != random_values[i])
            return 2;
    setupbody();
    setupitemtypes();
    setupbullettypes();
    setupobjecttypes_cars();
    setupobjecttypes_helicopters();
    setupobjectypes_train();
    setupcityobjecttypes();
    setupbuildingtypes();
    digest("tables");
    sr_server = 1;
    sr_game.ticks = 42;
    os_srand(1);
    human = createhuman(&pos.x, &rot[0].x, -1);
    if (human != 0)
        return 3;
    item = createitem(23, &pos, NULL, rot);
    if (item != 0 || !linkitem(item, UINT32_MAX, human, 0))
        return 4;
    createobject(0, &pos, NULL, rot, 0);
    sr_createevent_message(0, "storage test", -1, 0);
    sr_createevent_updatemission(0, 0);
    digest("entities");
    if (!boundaries())
        return 5;
    if (!client_accounts())
        return 6;
    puts("storage tests passed");
    return 0;
}
