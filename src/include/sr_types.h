/* Gerado por tools/re/gen_types.py. Não edite à mão:
 * edite analysis/structs/names.csv e rode o gerador de novo.
 *
 * Layout de 32 bits do Sub Rosa 0.24 (igual no Mach-O i386 e no PE do 024d).
 * Campos unk_XXXX: o jogo acessa o offset, mas o significado ainda não é conhecido.
 */
#ifndef SR_TYPES_H
#define SR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define SR_STATIC_ASSERT static_assert
#else
#define SR_STATIC_ASSERT _Static_assert
#endif

#pragma pack(push, 1)

typedef struct vec3 { float x, y, z; } vec3;

/* Osso do corpo (ragdoll). 15 por Human. Campos ainda não mapeados. */
typedef struct Bone { uint8_t raw[0xe0]; } Bone;

/* Posição do inventário. 8 por Human. */
typedef struct InventorySlot {
    int count;      /* +0x00 manager: quantidade */
    int item_id;    /* +0x04 manager: índice em items[] */
    uint8_t unk_08[0x1c];
} InventorySlot;

/* Conta salva em user/server.srk (docs/formatos.md). */
typedef struct Account {
    uint32_t id[2];     /* +0x00 igual a Player.account_id */
    char name[32];      /* +0x08 */
    int name_locked;    /* +0x28 não salvo no arquivo; createaccount grava 5 */
    int money;          /* +0x2c conta nova: 1000 */
    int unk_30;         /* +0x30 */
    int look_a;         /* +0x34 rand() % 3 */
    int look_b;         /* +0x38 rand() & 7 */
    int look_c;         /* +0x3c rand() & 3 */
    uint8_t unk_40[0xc];
} Account;

SR_STATIC_ASSERT(sizeof(vec3) == 12, "vec3");
SR_STATIC_ASSERT(sizeof(Bone) == 0xe0, "Bone");
SR_STATIC_ASSERT(sizeof(InventorySlot) == 0x24, "InventorySlot");
SR_STATIC_ASSERT(sizeof(Account) == 0x4c, "Account");
SR_STATIC_ASSERT(offsetof(Account, money) == 0x2c, "Account.money");


/* player[]: registro de 0x2408 bytes. */
typedef struct Player {
    int active;                          /* +0x0000 createplayer grava 1 */
    char name[32];                       /* +0x0004 createaccount copia 32 bytes para a conta */
    uint32_t account_id[2];              /* +0x0024 createaccount compara com conta+0x00 e +0x04 */
    int unk_002c;                        /* +0x002c 1 funções */
    int unk_0030;                        /* +0x0030 1 funções */
    int unk_0034;                        /* +0x0034 7 funções */
    int unk_0038;                        /* +0x0038 2 funções */
    int unk_003c;                        /* +0x003c 7 funções */
    int money;                           /* +0x0040 createplayer grava 50000; manager (black_market.c) usa como saldo; logic_gunstore_buy */
    int unk_0044;                        /* +0x0044 1 funções */
    float unk_0048;                      /* +0x0048 1 funções */
    int unk_004c;                        /* +0x004c 5 funções */
    int unk_0050;                        /* +0x0050 3 funções */
    int unk_0054;                        /* +0x0054 3 funções */
    int unk_0058;                        /* +0x0058 2 funções */
    int unk_005c;                        /* +0x005c 2 funções */
    int unk_0060;                        /* +0x0060 2 funções */
    int team;                            /* +0x0064 createplayer grava 7; createhuman copia para human.team; logic_addteamcash */
    int unk_0068;                        /* +0x0068 1 funções */
    int unk_006c;                        /* +0x006c 16 funções */
    int unk_0070;                        /* +0x0070 3 funções */
    int unk_0074;                        /* +0x0074 1 funções */
    int human_id;                        /* +0x0078 createplayer grava -1; manager: corpo do jogador */
    float unk_007c;                      /* +0x007c 9 funções */
    float unk_0080;                      /* +0x0080 7 funções */
    float unk_0084;                      /* +0x0084 8 funções */
    float unk_0088;                      /* +0x0088 8 funções */
    float yaw;                           /* +0x008c manager (ANALISE-NPCS.md): yaw do jogador */
    float unk_0090;                      /* +0x0090 5 funções */
    float unk_0094;                      /* +0x0094 2 funções */
    float unk_0098;                      /* +0x0098 2 funções */
    int unk_009c;                        /* +0x009c 10 funções */
    int unk_00a0;                        /* +0x00a0 1 funções */
    int unk_00a4;                        /* +0x00a4 2 funções */
    int unk_00a8;                        /* +0x00a8 2 funções */
    int unk_00ac;                        /* +0x00ac 7 funções */
    float unk_00b0;                      /* +0x00b0 4 funções */
    float unk_00b4;                      /* +0x00b4 4 funções */
    int unk_00b8;                        /* +0x00b8 2 funções */
    int unk_00bc;                        /* +0x00bc 1 funções */
    uint8_t pad_00c0[0x4];
    int unk_00c4;                        /* +0x00c4 2 funções */
    int unk_00c8;                        /* +0x00c8 6 funções */
    int unk_00cc;                        /* +0x00cc 5 funções */
    int unk_00d0;                        /* +0x00d0 1 funções */
    int unk_00d4;                        /* +0x00d4 16 funções */
    int unk_00d8;                        /* +0x00d8 8 funções */
    int unk_00dc;                        /* +0x00dc 1 funções */
    int unk_00e0;                        /* +0x00e0 12 funções */
    int unk_00e4;                        /* +0x00e4 8 funções */
    int unk_00e8;                        /* +0x00e8 4 funções */
    uint8_t pad_00ec[0x1500];
    int unk_15ec;                        /* +0x15ec 4 funções */
    int unk_15f0;                        /* +0x15f0 2 funções */
    int unk_15f4;                        /* +0x15f4 2 funções */
    int unk_15f8;                        /* +0x15f8 1 funções */
    int unk_15fc;                        /* +0x15fc 2 funções */
    uint8_t pad_1600[0x20];
    int unk_1620;                        /* +0x1620 1 funções */
    uint8_t pad_1624[0x414];
    int unk_1a38;                        /* +0x1a38 13 funções */
    int unk_1a3c;                        /* +0x1a3c 2 funções */
    int unk_1a40;                        /* +0x1a40 1 funções */
    int unk_1a44;                        /* +0x1a44 6 funções */
    int unk_1a48;                        /* +0x1a48 5 funções */
    uint8_t pad_1a4c[0x700];
    int unk_214c;                        /* +0x214c 3 funções */
    int unk_2150;                        /* +0x2150 3 funções */
    int unk_2154;                        /* +0x2154 3 funções */
    int unk_2158;                        /* +0x2158 1 funções */
    int unk_215c;                        /* +0x215c 1 funções */
    int unk_2160;                        /* +0x2160 1 funções */
    int unk_2164;                        /* +0x2164 1 funções */
    int unk_2168;                        /* +0x2168 2 funções */
    int unk_216c;                        /* +0x216c 2 funções */
    int unk_2170;                        /* +0x2170 1 funções */
    uint8_t pad_2174[0x17c];
    int unk_22f0;                        /* +0x22f0 3 funções */
    vec3 colors[4];                      /* +0x22f4 createplayer inicia 4 vetores; createhuman copia para human+0x2a5c (provável: cores da roupa) */
    uint8_t pad_2324[0x90];
    int look_a;                          /* +0x23b4 createplayer: rand() % 3; igual a conta.look_a */
    int look_b;                          /* +0x23b8 createplayer: rand() & 7 */
    int look_c;                          /* +0x23bc createplayer: rand() & 3 */
    int unk_23c0;                        /* +0x23c0 4 funções */
    int unk_23c4;                        /* +0x23c4 4 funções */
    int unk_23c8;                        /* +0x23c8 1 funções */
    uint8_t pad_23cc[0x3c];
} Player;
SR_STATIC_ASSERT(sizeof(Player) == 0x2408, "Player");
SR_STATIC_ASSERT(offsetof(Player, active) == 0x0, "Player.active");
SR_STATIC_ASSERT(offsetof(Player, name) == 0x4, "Player.name");
SR_STATIC_ASSERT(offsetof(Player, account_id) == 0x24, "Player.account_id");
SR_STATIC_ASSERT(offsetof(Player, money) == 0x40, "Player.money");
SR_STATIC_ASSERT(offsetof(Player, team) == 0x64, "Player.team");
SR_STATIC_ASSERT(offsetof(Player, human_id) == 0x78, "Player.human_id");
SR_STATIC_ASSERT(offsetof(Player, yaw) == 0x8c, "Player.yaw");
SR_STATIC_ASSERT(offsetof(Player, colors) == 0x22f4, "Player.colors");
SR_STATIC_ASSERT(offsetof(Player, look_a) == 0x23b4, "Player.look_a");
SR_STATIC_ASSERT(offsetof(Player, look_b) == 0x23b8, "Player.look_b");
SR_STATIC_ASSERT(offsetof(Player, look_c) == 0x23bc, "Player.look_c");

/* human[]: registro de 0x2b64 bytes. */
typedef struct Human {
    int active;                          /* +0x0000 createhuman grava 1 */
    int unk_0004;                        /* +0x0004 2 funções */
    int player_id;                       /* +0x0008 createhuman: parâmetro 3 (dono) */
    int vehicle_id;                      /* +0x000c createhuman grava -1 (provável: veículo) */
    int unk_0010;                        /* +0x0010 1 funções */
    int unk_0014;                        /* +0x0014 21 funções */
    int unk_0018;                        /* +0x0018 8 funções */
    int unk_001c;                        /* +0x001c 2 funções */
    int unk_0020;                        /* +0x0020 1 funções */
    int despawn_timer;                   /* +0x0024 createhuman grava 0xe10 (3600) (provável) */
    int health;                          /* +0x0028 createhuman grava 100; manager: vida */
    uint8_t pad_002c[0x4];
    int unk_0030;                        /* +0x0030 2 funções */
    int unk_0034;                        /* +0x0034 4 funções */
    int unk_0038;                        /* +0x0038 2 funções */
    int unk_003c;                        /* +0x003c 4 funções */
    uint8_t pad_0040[0x4];
    int unk_0044;                        /* +0x0044 2 funções */
    float unk_0048;                      /* +0x0048 14 funções */
    uint8_t pad_004c[0x4];
    float unk_0050;                      /* +0x0050 1 funções */
    int unk_0054;                        /* +0x0054 1 funções */
    uint8_t pad_0058[0x8];
    float unk_0060;                      /* +0x0060 6 funções */
    float unk_0064;                      /* +0x0064 5 funções */
    uint8_t pad_0068[0x18];
    float unk_0080;                      /* +0x0080 1 funções */
    float unk_0084;                      /* +0x0084 1 funções */
    uint8_t pad_0088[0x4];
    float unk_008c;                      /* +0x008c 4 funções */
    uint8_t pad_0090[0x8];
    float unk_0098;                      /* +0x0098 1 funções */
    int unk_009c;                        /* +0x009c 2 funções */
    int unk_00a0;                        /* +0x00a0 1 funções */
    int unk_00a4;                        /* +0x00a4 1 funções */
    int unk_00a8;                        /* +0x00a8 1 funções */
    float unk_00ac;                      /* +0x00ac 4 funções */
    float unk_00b0;                      /* +0x00b0 4 funções */
    float unk_00b4;                      /* +0x00b4 1 funções */
    float unk_00b8;                      /* +0x00b8 1 funções */
    int unk_00bc;                        /* +0x00bc 4 funções */
    int unk_00c0;                        /* +0x00c0 3 funções */
    uint8_t pad_00c4[0x4];
    Bone bones[15];                      /* +0x00c8 createhuman: loop de 15 com stride 0xe0 */
    uint8_t pad_0de8[0xe0];
    int unk_0ec8;                        /* +0x0ec8 4 funções */
    uint8_t pad_0ecc[0x1800];
    int unk_26cc;                        /* +0x26cc 4 funções */
    uint8_t pad_26d0[0x1a0];
    int unk_2870;                        /* +0x2870 1 funções */
    uint8_t pad_2874[0x48];
    int unk_28bc;                        /* +0x28bc 1 funções */
    uint8_t pad_28c0[0x50];
    float unk_2910;                      /* +0x2910 1 funções */
    float unk_2914;                      /* +0x2914 1 funções */
    float unk_2918;                      /* +0x2918 2 funções */
    InventorySlot inventory[8];          /* +0x291c createhuman: loop de 8 com stride 0x24; manager: contagem +0 e item +4 */
    int unk_2a3c;                        /* +0x2a3c 1 funções */
    uint8_t pad_2a40[0x1c];
    vec3 colors[4];                      /* +0x2a5c createhuman copia de player.colors */
    uint8_t pad_2a8c[0x90];
    int bleeding;                        /* +0x2b1c manager: faca grava 1 (provável: sangramento) */
    float unk_2b20;                      /* +0x2b20 2 funções */
    int unk_2b24;                        /* +0x2b24 1 funções */
    int unk_2b28;                        /* +0x2b28 1 funções */
    int team;                            /* +0x2b2c createhuman copia de player.team */
    int look_b;                          /* +0x2b30 createhuman copia de player.look_b */
    int look_c;                          /* +0x2b34 createhuman copia de player.look_c */
    int look_a;                          /* +0x2b38 createhuman copia de player.look_a */
    int unk_2b3c;                        /* +0x2b3c 2 funções */
    int unk_2b40;                        /* +0x2b40 2 funções */
    int unk_2b44;                        /* +0x2b44 1 funções */
    int unk_2b48;                        /* +0x2b48 1 funções */
    uint8_t pad_2b4c[0x8];
    int unk_2b54;                        /* +0x2b54 1 funções */
    int unk_2b58;                        /* +0x2b58 1 funções */
    uint8_t pad_2b5c[0x8];
} Human;
SR_STATIC_ASSERT(sizeof(Human) == 0x2b64, "Human");
SR_STATIC_ASSERT(offsetof(Human, active) == 0x0, "Human.active");
SR_STATIC_ASSERT(offsetof(Human, player_id) == 0x8, "Human.player_id");
SR_STATIC_ASSERT(offsetof(Human, vehicle_id) == 0xc, "Human.vehicle_id");
SR_STATIC_ASSERT(offsetof(Human, despawn_timer) == 0x24, "Human.despawn_timer");
SR_STATIC_ASSERT(offsetof(Human, health) == 0x28, "Human.health");
SR_STATIC_ASSERT(offsetof(Human, bones) == 0xc8, "Human.bones");
SR_STATIC_ASSERT(offsetof(Human, inventory) == 0x291c, "Human.inventory");
SR_STATIC_ASSERT(offsetof(Human, colors) == 0x2a5c, "Human.colors");
SR_STATIC_ASSERT(offsetof(Human, bleeding) == 0x2b1c, "Human.bleeding");
SR_STATIC_ASSERT(offsetof(Human, team) == 0x2b2c, "Human.team");
SR_STATIC_ASSERT(offsetof(Human, look_b) == 0x2b30, "Human.look_b");
SR_STATIC_ASSERT(offsetof(Human, look_c) == 0x2b34, "Human.look_c");
SR_STATIC_ASSERT(offsetof(Human, look_a) == 0x2b38, "Human.look_a");

/* item[]: registro de 0x1ac bytes. */
typedef struct Item {
    int active;                          /* +0x0000 createitem grava 1 */
    int unk_0004;                        /* +0x0004 3 funções */
    int unk_0008;                        /* +0x0008 2 funções */
    int type;                            /* +0x000c createitem: parâmetro 1 (tipo do item) */
    int type_flags;                      /* +0x0010 createitem: itemtype+4 */
    int unk_14;                          /* +0x0014 createitem grava 0xffff */
    int parent_human;                    /* +0x0018 createitem grava -1 (provável) */
    int parent_item;                     /* +0x001c createitem grava -1 (provável) */
    int unk_0020;                        /* +0x0020 13 funções */
    int unk_0024;                        /* +0x0024 7 funções */
    int unk_0028;                        /* +0x0028 3 funções */
    uint8_t pad_002c[0xc];
    int state;                           /* +0x0038 manager: estado 3 = telefone especial */
    uint8_t pad_003c[0xc];
    vec3 pos;                            /* +0x0048 createitem: parâmetro 2 (posição) */
    vec3 pos_prev;                       /* +0x0054 createitem: cópia da posição */
    int unk_0060;                        /* +0x0060 1 funções */
    uint8_t pad_0064[0x8];
    vec3 vel;                            /* +0x006c createitem: parâmetro 3 (velocidade) */
    int unk_0078;                        /* +0x0078 2 funções */
    uint8_t pad_007c[0x8];
    vec3 rot[3];                         /* +0x0084 createitem: parâmetro 4 (matriz 3x3) */
    int unk_00a8;                        /* +0x00a8 4 funções */
    uint8_t pad_00ac[0x8];
    int unk_00b4;                        /* +0x00b4 4 funções */
    uint8_t pad_00b8[0x8];
    int unk_00c0;                        /* +0x00c0 1 funções */
    uint8_t pad_00c4[0x8];
    int unk_00cc;                        /* +0x00cc 2 funções */
    uint8_t pad_00d0[0x8];
    int unk_00d8;                        /* +0x00d8 3 funções */
    int unk_00dc;                        /* +0x00dc 1 funções */
    int unk_00e0;                        /* +0x00e0 6 funções */
    int unk_00e4;                        /* +0x00e4 1 funções */
    int unk_00e8;                        /* +0x00e8 1 funções */
    int unk_00ec;                        /* +0x00ec 3 funções */
    int unk_00f0;                        /* +0x00f0 1 funções */
    int unk_00f4;                        /* +0x00f4 3 funções */
    int unk_00f8;                        /* +0x00f8 5 funções */
    int unk_00fc;                        /* +0x00fc 5 funções */
    int unk_0100;                        /* +0x0100 1 funções */
    int unk_0104;                        /* +0x0104 4 funções */
    int phone_number;                    /* +0x0108 manager: número discado */
    int unk_010c;                        /* +0x010c 5 funções */
    int call_state;                      /* +0x0110 manager: estado da ligação */
    int unk_0114;                        /* +0x0114 10 funções */
    float unk_0118;                      /* +0x0118 2 funções */
    float unk_011c;                      /* +0x011c 2 funções */
    float unk_0120;                      /* +0x0120 2 funções */
    float unk_0124;                      /* +0x0124 3 funções */
    float unk_0128;                      /* +0x0128 3 funções */
    float unk_012c;                      /* +0x012c 3 funções */
    int unk_0130;                        /* +0x0130 1 funções */
    int unk_0134;                        /* +0x0134 6 funções */
    int unk_0138;                        /* +0x0138 9 funções */
    int unk_013c;                        /* +0x013c 8 funções */
    uint8_t pad_0140[0x3c];
    int unk_017c;                        /* +0x017c 1 funções */
    int unk_0180;                        /* +0x0180 1 funções */
    int unk_0184;                        /* +0x0184 1 funções */
    int unk_0188;                        /* +0x0188 1 funções */
    float unk_018c;                      /* +0x018c 1 funções */
    int unk_0190;                        /* +0x0190 1 funções */
    int unk_0194;                        /* +0x0194 2 funções */
    uint8_t pad_0198[0x8];
    int unk_01a0;                        /* +0x01a0 1 funções */
    uint8_t pad_01a4[0x8];
} Item;
SR_STATIC_ASSERT(sizeof(Item) == 0x1ac, "Item");
SR_STATIC_ASSERT(offsetof(Item, active) == 0x0, "Item.active");
SR_STATIC_ASSERT(offsetof(Item, type) == 0xc, "Item.type");
SR_STATIC_ASSERT(offsetof(Item, type_flags) == 0x10, "Item.type_flags");
SR_STATIC_ASSERT(offsetof(Item, unk_14) == 0x14, "Item.unk_14");
SR_STATIC_ASSERT(offsetof(Item, parent_human) == 0x18, "Item.parent_human");
SR_STATIC_ASSERT(offsetof(Item, parent_item) == 0x1c, "Item.parent_item");
SR_STATIC_ASSERT(offsetof(Item, state) == 0x38, "Item.state");
SR_STATIC_ASSERT(offsetof(Item, pos) == 0x48, "Item.pos");
SR_STATIC_ASSERT(offsetof(Item, pos_prev) == 0x54, "Item.pos_prev");
SR_STATIC_ASSERT(offsetof(Item, vel) == 0x6c, "Item.vel");
SR_STATIC_ASSERT(offsetof(Item, rot) == 0x84, "Item.rot");
SR_STATIC_ASSERT(offsetof(Item, phone_number) == 0x108, "Item.phone_number");
SR_STATIC_ASSERT(offsetof(Item, call_state) == 0x110, "Item.call_state");

/* object[]: registro de 0x8230 bytes. */
typedef struct Object {
    int active;                          /* +0x0000 manager: slot livre com flag 0 */
    int unk_0004;                        /* +0x0004 15 funções */
    int unk_0008;                        /* +0x0008 6 funções */
    int unk_000c;                        /* +0x000c 7 funções */
    uint8_t pad_0010[0x4];
    int unk_0014;                        /* +0x0014 2 funções */
    int unk_0018;                        /* +0x0018 4 funções */
    int unk_001c;                        /* +0x001c 4 funções */
    int unk_0020;                        /* +0x0020 3 funções */
    int unk_0024;                        /* +0x0024 20 funções */
    float unk_0028;                      /* +0x0028 1 funções */
    uint8_t pad_002c[0x4];
    int unk_0030;                        /* +0x0030 2 funções */
    uint8_t pad_0034[0x8];
    int unk_003c;                        /* +0x003c 20 funções */
    uint8_t pad_0040[0x8];
    int unk_0048;                        /* +0x0048 19 funções */
    uint8_t pad_004c[0x8];
    int unk_0054;                        /* +0x0054 19 funções */
    uint8_t pad_0058[0x4];
    float unk_005c;                      /* +0x005c 1 funções */
    int unk_0060;                        /* +0x0060 9 funções */
    int unk_0064;                        /* +0x0064 3 funções */
    int unk_0068;                        /* +0x0068 3 funções */
    int unk_006c;                        /* +0x006c 1 funções */
    uint8_t pad_0070[0x8];
    float unk_0078;                      /* +0x0078 1 funções */
    float unk_007c;                      /* +0x007c 1 funções */
    float unk_0080;                      /* +0x0080 1 funções */
    float unk_0084;                      /* +0x0084 5 funções */
    float unk_0088;                      /* +0x0088 4 funções */
    float unk_008c;                      /* +0x008c 5 funções */
    float unk_0090;                      /* +0x0090 5 funções */
    float unk_0094;                      /* +0x0094 4 funções */
    float unk_0098;                      /* +0x0098 5 funções */
    uint8_t pad_009c[0xc];
    int unk_00a8;                        /* +0x00a8 8 funções */
    uint8_t pad_00ac[0x18];
    int unk_00c4;                        /* +0x00c4 1 funções */
    int unk_00c8;                        /* +0x00c8 1 funções */
    uint8_t pad_00cc[0x30];
    int unk_00fc;                        /* +0x00fc 2 funções */
    int unk_0100;                        /* +0x0100 1 funções */
    uint8_t pad_0104[0xa8];
    int unk_01ac;                        /* +0x01ac 2 funções */
    uint8_t pad_01b0[0x200];
    int unk_03b0;                        /* +0x03b0 2 funções */
    uint8_t pad_03b4[0x400];
    int unk_07b4;                        /* +0x07b4 3 funções */
    uint8_t pad_07b8[0x5404];
    float unk_5bbc;                      /* +0x5bbc 4 funções */
    float unk_5bc0;                      /* +0x5bc0 3 funções */
    float unk_5bc4;                      /* +0x5bc4 4 funções */
    float unk_5bc8;                      /* +0x5bc8 4 funções */
    uint8_t pad_5bcc[0x10];
    int unk_5bdc;                        /* +0x5bdc 2 funções */
    uint8_t pad_5be0[0x4];
    int unk_5be4;                        /* +0x5be4 3 funções */
    uint8_t pad_5be8[0x264];
    float unk_5e4c;                      /* +0x5e4c 2 funções */
    float unk_5e50;                      /* +0x5e50 4 funções */
    float unk_5e54;                      /* +0x5e54 2 funções */
    float unk_5e58;                      /* +0x5e58 2 funções */
    float unk_5e5c;                      /* +0x5e5c 1 funções */
    float unk_5e60;                      /* +0x5e60 2 funções */
    float unk_5e64;                      /* +0x5e64 1 funções */
    float unk_5e68;                      /* +0x5e68 2 funções */
    int unk_5e6c;                        /* +0x5e6c 2 funções */
    int unk_5e70;                        /* +0x5e70 1 funções */
    uint8_t pad_5e74[0x4];
    float unk_5e78;                      /* +0x5e78 2 funções */
    float unk_5e7c;                      /* +0x5e7c 2 funções */
    float unk_5e80;                      /* +0x5e80 2 funções */
    float unk_5e84;                      /* +0x5e84 2 funções */
    uint8_t pad_5e88[0x8];
    int unk_5e90;                        /* +0x5e90 2 funções */
    int unk_5e94;                        /* +0x5e94 2 funções */
    int unk_5e98;                        /* +0x5e98 2 funções */
    uint8_t pad_5e9c[0x44];
    int unk_5ee0;                        /* +0x5ee0 6 funções */
    int unk_5ee4;                        /* +0x5ee4 1 funções */
    int unk_5ee8;                        /* +0x5ee8 7 funções */
    uint8_t pad_5eec[0x30];
    float unk_5f1c;                      /* +0x5f1c 5 funções */
    float unk_5f20;                      /* +0x5f20 5 funções */
    uint8_t pad_5f24[0x24];
    int unk_5f48;                        /* +0x5f48 3 funções */
    uint8_t pad_5f4c[0x10];
    float unk_5f5c;                      /* +0x5f5c 4 funções */
    uint8_t pad_5f60[0x54];
    float unk_5fb4;                      /* +0x5fb4 4 funções */
    float unk_5fb8;                      /* +0x5fb8 5 funções */
    uint8_t pad_5fbc[0x24];
    int unk_5fe0;                        /* +0x5fe0 3 funções */
    uint8_t pad_5fe4[0x10];
    float unk_5ff4;                      /* +0x5ff4 4 funções */
    uint8_t pad_5ff8[0x54];
    int unk_604c;                        /* +0x604c 3 funções */
    uint8_t pad_6050[0x28];
    int unk_6078;                        /* +0x6078 3 funções */
    uint8_t pad_607c[0x10];
    float unk_608c;                      /* +0x608c 4 funções */
    uint8_t pad_6090[0x54];
    int unk_60e4;                        /* +0x60e4 3 funções */
    uint8_t pad_60e8[0x28];
    int unk_6110;                        /* +0x6110 3 funções */
    uint8_t pad_6114[0x10];
    float unk_6124;                      /* +0x6124 4 funções */
    uint8_t pad_6128[0x10c4];
    int unk_71ec;                        /* +0x71ec 6 funções */
    uint8_t pad_71f0[0xe84];
    int unk_8074;                        /* +0x8074 2 funções */
    uint8_t pad_8078[0x20];
    int unk_8098;                        /* +0x8098 1 funções */
    uint8_t pad_809c[0x20];
    int unk_80bc;                        /* +0x80bc 1 funções */
    uint8_t pad_80c0[0x20];
    int unk_80e0;                        /* +0x80e0 1 funções */
    uint8_t pad_80e4[0x20];
    int unk_8104;                        /* +0x8104 2 funções */
    uint8_t pad_8108[0x8];
    int unk_8110;                        /* +0x8110 1 funções */
    uint8_t pad_8114[0x8];
    int unk_811c;                        /* +0x811c 1 funções */
    uint8_t pad_8120[0x8];
    int unk_8128;                        /* +0x8128 2 funções */
    uint8_t pad_812c[0x68];
    int unk_8194;                        /* +0x8194 3 funções */
    int unk_8198;                        /* +0x8198 1 funções */
    int unk_819c;                        /* +0x819c 2 funções */
    float unk_81a0;                      /* +0x81a0 2 funções */
    float unk_81a4;                      /* +0x81a4 2 funções */
    int unk_81a8;                        /* +0x81a8 2 funções */
    uint8_t pad_81ac[0x60];
    float unk_820c;                      /* +0x820c 1 funções */
    int unk_8210;                        /* +0x8210 2 funções */
    int unk_8214;                        /* +0x8214 2 funções */
    uint8_t pad_8218[0x8];
    int unk_8220;                        /* +0x8220 2 funções */
    int unk_8224;                        /* +0x8224 2 funções */
    uint8_t pad_8228[0x8];
} Object;
SR_STATIC_ASSERT(sizeof(Object) == 0x8230, "Object");
SR_STATIC_ASSERT(offsetof(Object, active) == 0x0, "Object.active");

/* connection[]: registro de 0xf170 bytes. */
typedef struct Connection {
    uint32_t ip;                         /* +0x0000 manager (ANALISE-NPCS.md) */
    int port;                            /* +0x0004 manager (ANALISE-NPCS.md) */
    int unk_0008;                        /* +0x0008 2 funções */
    int unk_000c;                        /* +0x000c 2 funções */
    int player_id;                       /* +0x0010 manager (ANALISE-NPCS.md) */
    int unk_0014;                        /* +0x0014 2 funções */
    int unk_0018;                        /* +0x0018 1 funções */
    int unk_001c;                        /* +0x001c 3 funções */
    uint8_t pad_0020[0x24];
    int unk_0044;                        /* +0x0044 5 funções */
    uint8_t pad_0048[0x4];
    int unk_004c;                        /* +0x004c 2 funções */
    uint8_t pad_0050[0x104];
    int unk_0154;                        /* +0x0154 2 funções */
    uint8_t pad_0158[0xf000];
    int unk_f158;                        /* +0xf158 4 funções */
    uint8_t pad_f15c[0x8];
    int unk_f164;                        /* +0xf164 2 funções */
    uint8_t pad_f168[0x8];
} Connection;
SR_STATIC_ASSERT(sizeof(Connection) == 0xf170, "Connection");
SR_STATIC_ASSERT(offsetof(Connection, ip) == 0x0, "Connection.ip");
SR_STATIC_ASSERT(offsetof(Connection, port) == 0x4, "Connection.port");
SR_STATIC_ASSERT(offsetof(Connection, player_id) == 0x10, "Connection.player_id");

#pragma pack(pop)

#endif /* SR_TYPES_H */
