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

/* Osso do corpo (ragdoll). 15 por Human, a partir de Human+0xc8.
 * Fonte: createhuman e bondsimulation2 (Mach-O). */
typedef struct Bone {
    vec3 pos;           /* +0x00 createhuman: posição */
    vec3 pos_prev;      /* +0x0c createhuman: cópia da posição */
    uint8_t unk_18[0x18];
    vec3 rot[3];        /* +0x30 createhuman: matriz do parâmetro 2; bondsimulation2 usa as 3 linhas */
    uint8_t unk_54[0x38];
    float mass;         /* +0x8c bondsimulation2: proporção de massa com Item.mass */
    uint8_t unk_90[0x50];
} Bone;

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
SR_STATIC_ASSERT(offsetof(Bone, rot) == 0x30, "Bone.rot");
SR_STATIC_ASSERT(offsetof(Bone, mass) == 0x8c, "Bone.mass");
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
    int num_purchases;                   /* +0x004c logic_gunstore_buy e logic_playeractions: limite < 10 */
    int unk_0050;                        /* +0x0050 3 funções */
    int unk_0054;                        /* +0x0054 3 funções */
    int unk_0058;                        /* +0x0058 2 funções */
    int unk_005c;                        /* +0x005c 2 funções */
    int unk_0060;                        /* +0x0060 2 funções */
    int team;                            /* +0x0064 createplayer grava 7; createhuman copia para human.team; logic_addteamcash */
    int unk_0068;                        /* +0x0068 1 funções */
    int stocks;                          /* +0x006c playerbuystock/playersellstock; economysimulation */
    int unk_0070;                        /* +0x0070 3 funções */
    int unk_0074;                        /* +0x0074 1 funções */
    int human_id;                        /* +0x0078 createplayer grava -1; manager: corpo do jogador */
    float input_axes[4];                 /* +0x007c getinputs_human/car/heli gravam floats */
    float yaw;                           /* +0x008c manager (ANALISE-NPCS.md): yaw do jogador */
    float input_pitch;                   /* +0x0090 getinputs_* gravam; vizinho de yaw (provável) */
    float input_extra[2];                /* +0x0094 serverrecv lê da rede junto com os eixos (2 floats depois de input_pitch) */
    uint32_t input_buttons;              /* +0x009c getinputs_* gravam bits */
    int unk_00a0;                        /* +0x00a0 1 funções */
    int unk_00a4;                        /* +0x00a4 2 funções */
    int unk_00a8;                        /* +0x00a8 2 funções */
    int hud_mode;                        /* +0x00ac getinputs_human soma 1; drawhud compara com 0 e 1 */
    float unk_00b0;                      /* +0x00b0 4 funções */
    float unk_00b4;                      /* +0x00b4 4 funções */
    int unk_00b8;                        /* +0x00b8 2 funções */
    int unk_00bc;                        /* +0x00bc 1 funções */
    uint8_t pad_00c0[0x4];
    int unk_00c4;                        /* +0x00c4 2 funções */
    int input_mode;                      /* +0x00c8 serverrecv: 8 bits; getinputs grava 0/1; playersimulation compara 1 e 2 (provável) */
    int unk_00cc;                        /* +0x00cc 5 funções */
    int unk_00d0;                        /* +0x00d0 1 funções */
    int menu;                            /* +0x00d4 playermenu compara com 1 e 2; logic_player grava 9 */
    int menu_building;                   /* +0x00d8 logic_cardealer_buy usa como índice (passo 0x9dc); serversend envia 16 bits */
    int unk_00dc;                        /* +0x00dc 1 funções */
    int unk_00e0;                        /* +0x00e0 12 funções */
    int num_actions;                     /* +0x00e4 player_addaction_chat/menu: índice de ações (passo 0x54) */
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
    int is_bot;                          /* +0x1a38 scenario_createchase grava 1; logicsimulation_round filtra == 0; manager: bots não compram */
    int unk_1a3c;                        /* +0x1a3c 2 funções */
    int unk_1a40;                        /* +0x1a40 1 funções */
    int ai_waypoint;                     /* +0x1a44 ai_playercar/ai_car_nextwaypoint: índice no caminho (passo 0x1c em +0x1a58) */
    int num_path;                        /* +0x1a48 resetgame/scenario_createtraffic2: contador da lista em +0x1a4c (passo 0x1c) */
    uint8_t pad_1a4c[0x700];
    int unk_214c;                        /* +0x214c 3 funções */
    int unk_2150;                        /* +0x2150 3 funções */
    int unk_2154;                        /* +0x2154 3 funções */
    int unk_2158;                        /* +0x2158 1 funções */
    int unk_215c;                        /* +0x215c 1 funções */
    int unk_2160;                        /* +0x2160 1 funções */
    int unk_2164;                        /* +0x2164 1 funções */
    int num_threats;                     /* +0x2168 ai_assessthreats: laço e limite < 0x10 */
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
SR_STATIC_ASSERT(offsetof(Player, num_purchases) == 0x4c, "Player.num_purchases");
SR_STATIC_ASSERT(offsetof(Player, team) == 0x64, "Player.team");
SR_STATIC_ASSERT(offsetof(Player, stocks) == 0x6c, "Player.stocks");
SR_STATIC_ASSERT(offsetof(Player, human_id) == 0x78, "Player.human_id");
SR_STATIC_ASSERT(offsetof(Player, input_axes) == 0x7c, "Player.input_axes");
SR_STATIC_ASSERT(offsetof(Player, yaw) == 0x8c, "Player.yaw");
SR_STATIC_ASSERT(offsetof(Player, input_pitch) == 0x90, "Player.input_pitch");
SR_STATIC_ASSERT(offsetof(Player, input_extra) == 0x94, "Player.input_extra");
SR_STATIC_ASSERT(offsetof(Player, input_buttons) == 0x9c, "Player.input_buttons");
SR_STATIC_ASSERT(offsetof(Player, hud_mode) == 0xac, "Player.hud_mode");
SR_STATIC_ASSERT(offsetof(Player, input_mode) == 0xc8, "Player.input_mode");
SR_STATIC_ASSERT(offsetof(Player, menu) == 0xd4, "Player.menu");
SR_STATIC_ASSERT(offsetof(Player, menu_building) == 0xd8, "Player.menu_building");
SR_STATIC_ASSERT(offsetof(Player, num_actions) == 0xe4, "Player.num_actions");
SR_STATIC_ASSERT(offsetof(Player, is_bot) == 0x1a38, "Player.is_bot");
SR_STATIC_ASSERT(offsetof(Player, ai_waypoint) == 0x1a44, "Player.ai_waypoint");
SR_STATIC_ASSERT(offsetof(Player, num_path) == 0x1a48, "Player.num_path");
SR_STATIC_ASSERT(offsetof(Player, num_threats) == 0x2168, "Player.num_threats");
SR_STATIC_ASSERT(offsetof(Player, colors) == 0x22f4, "Player.colors");
SR_STATIC_ASSERT(offsetof(Player, look_a) == 0x23b4, "Player.look_a");
SR_STATIC_ASSERT(offsetof(Player, look_b) == 0x23b8, "Player.look_b");
SR_STATIC_ASSERT(offsetof(Player, look_c) == 0x23bc, "Player.look_c");

/* human[]: registro de 0x2b64 bytes. */
typedef struct Human {
    int active;                          /* +0x0000 createhuman grava 1 */
    int unk_0004;                        /* +0x0004 2 funções */
    int player_id;                       /* +0x0008 createhuman: parâmetro 3 (dono) */
    int unk_000c;                        /* +0x000c 3 funções */
    int unk_0010;                        /* +0x0010 1 funções */
    int vehicle_id;                      /* +0x0014 createhuman grava -1; packobjectpacket envia se != -1 */
    int vehicle_seat;                    /* +0x0018 playerai compara com 0 (motorista) e 1 */
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
    vec3 pos;                            /* +0x0048 manager: posição global; logic_playeractions usa em pointinsidebase */
    int unk_0054;                        /* +0x0054 1 funções */
    uint8_t pad_0058[0x8];
    float axis;                          /* +0x0060 getinputs_human: rótulo de debug "humanaxis" */
    float pitch;                         /* +0x0064 human_calculatearmangles multiplica por constante (provável: ângulo vertical) */
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
    uint32_t input_buttons;              /* +0x00bc humansimulation testa bits &1 &8 &0x10 */
    uint32_t input_buttons_prev;         /* +0x00c0 createhuman grava 0x3c0; testes de bits como +0xbc (provável: botões do quadro anterior) */
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
SR_STATIC_ASSERT(offsetof(Human, vehicle_id) == 0x14, "Human.vehicle_id");
SR_STATIC_ASSERT(offsetof(Human, vehicle_seat) == 0x18, "Human.vehicle_seat");
SR_STATIC_ASSERT(offsetof(Human, despawn_timer) == 0x24, "Human.despawn_timer");
SR_STATIC_ASSERT(offsetof(Human, health) == 0x28, "Human.health");
SR_STATIC_ASSERT(offsetof(Human, pos) == 0x48, "Human.pos");
SR_STATIC_ASSERT(offsetof(Human, axis) == 0x60, "Human.axis");
SR_STATIC_ASSERT(offsetof(Human, pitch) == 0x64, "Human.pitch");
SR_STATIC_ASSERT(offsetof(Human, input_buttons) == 0xbc, "Human.input_buttons");
SR_STATIC_ASSERT(offsetof(Human, input_buttons_prev) == 0xc0, "Human.input_buttons_prev");
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
    float mass;                          /* +0x0010 createitem: itemtype+4; bondsimulation2 usa em proporção de massa com Bone.mass */
    int unk_14;                          /* +0x0014 createitem grava 0xffff */
    int parent_human;                    /* +0x0018 createitem grava -1 (provável) */
    int parent_item;                     /* +0x001c createitem grava -1 (provável) */
    int parent_slot;                     /* +0x0020 clientrecv: índice * 0x24 + 0x291c (InventorySlot do corpo) */
    int num_children;                    /* +0x0024 linkitemtoitem: < 8; deleteitem: laço */
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
    vec3 angvel;                         /* +0x00a8 item_physicssimulation amortece com scalevector; itemsimulation usa em dotproduct */
    vec3 torque;                         /* +0x00b4 itemsimulation zera e acumula por quadro; bondsimulation2 soma crossproduct (provável) */
    int unk_00c0;                        /* +0x00c0 1 funções */
    uint8_t pad_00c4[0x8];
    int unk_00cc;                        /* +0x00cc 2 funções */
    uint8_t pad_00d0[0x8];
    int unk_00d8;                        /* +0x00d8 3 funções */
    int unk_00dc;                        /* +0x00dc 1 funções */
    int cooldown;                        /* +0x00e0 createitem: itemtype+0x1c; itemlogic diminui 1; itemweaponsimulation testa == 0 */
    int unk_e4;                          /* +0x00e4 createitem: itemtype+0x18 */
    int unk_00e8;                        /* +0x00e8 1 funções */
    uint32_t flags;                      /* +0x00ec itemlogic testa bit 1; human_itemsimulation liga bit 1 */
    int unk_00f0;                        /* +0x00f0 1 funções */
    int unk_00f4;                        /* +0x00f4 3 funções */
    int linked_item;                     /* +0x00f8 itemlogic grava índice de item; logic_setteammanager zera call_state do item ligado */
    int phone_id;                        /* +0x00fc logic_setteammanager compara com time * 1111 (0x457) */
    int unk_0100;                        /* +0x0100 1 funções */
    int info;                            /* +0x0104 createevent_updateiteminfo envia; itemlogic e eventsimulation gravam */
    int phone_number;                    /* +0x0108 manager: número discado */
    int unk_010c;                        /* +0x010c 5 funções */
    int call_state;                      /* +0x0110 manager: estado da ligação */
    int vehicle_id;                      /* +0x0114 player_calcassets: < 0x400; economysimulation: índice * 0x8230 (provável: chave) */
    float unk_0118;                      /* +0x0118 2 funções */
    float unk_011c;                      /* +0x011c 2 funções */
    float unk_0120;                      /* +0x0120 2 funções */
    float unk_0124;                      /* +0x0124 3 funções */
    float unk_0128;                      /* +0x0128 3 funções */
    float unk_012c;                      /* +0x012c 3 funções */
    int unk_0130;                        /* +0x0130 1 funções */
    int num_bills;                       /* +0x0134 item_addbill e logic_playeractions somam 1 */
    int cash;                            /* +0x0138 item_removebill/logic_playeractions comparam valor */
    uint32_t bills_mask;                 /* +0x013c item_addbill/resetgame alteram bits */
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
SR_STATIC_ASSERT(offsetof(Item, mass) == 0x10, "Item.mass");
SR_STATIC_ASSERT(offsetof(Item, unk_14) == 0x14, "Item.unk_14");
SR_STATIC_ASSERT(offsetof(Item, parent_human) == 0x18, "Item.parent_human");
SR_STATIC_ASSERT(offsetof(Item, parent_item) == 0x1c, "Item.parent_item");
SR_STATIC_ASSERT(offsetof(Item, parent_slot) == 0x20, "Item.parent_slot");
SR_STATIC_ASSERT(offsetof(Item, num_children) == 0x24, "Item.num_children");
SR_STATIC_ASSERT(offsetof(Item, state) == 0x38, "Item.state");
SR_STATIC_ASSERT(offsetof(Item, pos) == 0x48, "Item.pos");
SR_STATIC_ASSERT(offsetof(Item, pos_prev) == 0x54, "Item.pos_prev");
SR_STATIC_ASSERT(offsetof(Item, vel) == 0x6c, "Item.vel");
SR_STATIC_ASSERT(offsetof(Item, rot) == 0x84, "Item.rot");
SR_STATIC_ASSERT(offsetof(Item, angvel) == 0xa8, "Item.angvel");
SR_STATIC_ASSERT(offsetof(Item, torque) == 0xb4, "Item.torque");
SR_STATIC_ASSERT(offsetof(Item, cooldown) == 0xe0, "Item.cooldown");
SR_STATIC_ASSERT(offsetof(Item, unk_e4) == 0xe4, "Item.unk_e4");
SR_STATIC_ASSERT(offsetof(Item, flags) == 0xec, "Item.flags");
SR_STATIC_ASSERT(offsetof(Item, linked_item) == 0xf8, "Item.linked_item");
SR_STATIC_ASSERT(offsetof(Item, phone_id) == 0xfc, "Item.phone_id");
SR_STATIC_ASSERT(offsetof(Item, info) == 0x104, "Item.info");
SR_STATIC_ASSERT(offsetof(Item, phone_number) == 0x108, "Item.phone_number");
SR_STATIC_ASSERT(offsetof(Item, call_state) == 0x110, "Item.call_state");
SR_STATIC_ASSERT(offsetof(Item, vehicle_id) == 0x114, "Item.vehicle_id");
SR_STATIC_ASSERT(offsetof(Item, num_bills) == 0x134, "Item.num_bills");
SR_STATIC_ASSERT(offsetof(Item, cash) == 0x138, "Item.cash");
SR_STATIC_ASSERT(offsetof(Item, bills_mask) == 0x13c, "Item.bills_mask");

/* object[]: registro de 0x8230 bytes. */
typedef struct Object {
    int active;                          /* +0x0000 manager: slot livre com flag 0 */
    int type;                            /* +0x0004 createobject: parâmetro 1; índice em objecttype */
    int state;                           /* +0x0008 objectsimulation compara com 1 e 2; packobjectpacket ignora 3 (provável: estado do objeto) */
    int health;                          /* +0x000c createobject grava 100 (provável) */
    uint8_t pad_0010[0x4];
    int driver;                          /* +0x0014 createobject grava -1 (provável) */
    int unk_0018;                        /* +0x0018 4 funções */
    int unk_001c;                        /* +0x001c 4 funções */
    int unk_0020;                        /* +0x0020 3 funções */
    vec3 pos;                            /* +0x0024 createobject: parâmetro 2 */
    vec3 pos_prev;                       /* +0x0030 createobject: cópia de pos */
    vec3 rot[3];                         /* +0x003c createobject: matriz do parâmetro 4; objectsimulation normaliza e faz crossproduct */
    vec3 vel;                            /* +0x0060 objectsimulation zera */
    vec3 vel_prev;                       /* +0x006c objectsimulation copia de vel */
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
    int particle_start;                  /* +0x00a8 createobject/deleteparticle/objectsimulation: início das partículas do objeto */
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
    float engine_accel;                  /* +0x5e4c objectenginesimulation2: calculado */
    float engine_rpm;                    /* +0x5e50 getenginetorque2/objectenginesimulation2 (provável) */
    float throttle;                      /* +0x5e54 objectenginesimulation2: entra como (1 - x) na aceleração (provável) */
    float engine_power;                  /* +0x5e58 createobject: objecttype+0x7758 * constante; manager (truck_speed) altera a força do motor */
    float unk_5e5c;                      /* +0x5e5c 1 funções */
    float unk_5e60;                      /* +0x5e60 2 funções */
    float unk_5e64;                      /* +0x5e64 1 funções */
    float unk_5e68;                      /* +0x5e68 2 funções */
    int gear;                            /* +0x5e6c objectsimulation_car grava 1 e 3; ai_playercar compara > 1 (provável) */
    int unk_5e70;                        /* +0x5e70 1 funções */
    uint8_t pad_5e74[0x4];
    float gear_ratio;                    /* +0x5e78 objectenginesimulation2: se != 0 acopla motor e rodas; objectsimulation_car grava por marcha (provável) */
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
    int num_wheels;                      /* +0x5ee8 createobject grava; objectsimulation usa como limite de laço (provável: cópia de objecttype.num_wheels) */
    uint8_t pad_5eec[0x30];
    float rotor_angle;                   /* +0x5f1c objectsimulation_heli atualiza; unpackobjectpacket lê com unpackangle */
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
    int num_seats;                       /* +0x81a8 createobject copia de objecttype.num_seats; humansimulation lê */
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
SR_STATIC_ASSERT(offsetof(Object, type) == 0x4, "Object.type");
SR_STATIC_ASSERT(offsetof(Object, state) == 0x8, "Object.state");
SR_STATIC_ASSERT(offsetof(Object, health) == 0xc, "Object.health");
SR_STATIC_ASSERT(offsetof(Object, driver) == 0x14, "Object.driver");
SR_STATIC_ASSERT(offsetof(Object, pos) == 0x24, "Object.pos");
SR_STATIC_ASSERT(offsetof(Object, pos_prev) == 0x30, "Object.pos_prev");
SR_STATIC_ASSERT(offsetof(Object, rot) == 0x3c, "Object.rot");
SR_STATIC_ASSERT(offsetof(Object, vel) == 0x60, "Object.vel");
SR_STATIC_ASSERT(offsetof(Object, vel_prev) == 0x6c, "Object.vel_prev");
SR_STATIC_ASSERT(offsetof(Object, particle_start) == 0xa8, "Object.particle_start");
SR_STATIC_ASSERT(offsetof(Object, engine_accel) == 0x5e4c, "Object.engine_accel");
SR_STATIC_ASSERT(offsetof(Object, engine_rpm) == 0x5e50, "Object.engine_rpm");
SR_STATIC_ASSERT(offsetof(Object, throttle) == 0x5e54, "Object.throttle");
SR_STATIC_ASSERT(offsetof(Object, engine_power) == 0x5e58, "Object.engine_power");
SR_STATIC_ASSERT(offsetof(Object, gear) == 0x5e6c, "Object.gear");
SR_STATIC_ASSERT(offsetof(Object, gear_ratio) == 0x5e78, "Object.gear_ratio");
SR_STATIC_ASSERT(offsetof(Object, num_wheels) == 0x5ee8, "Object.num_wheels");
SR_STATIC_ASSERT(offsetof(Object, rotor_angle) == 0x5f1c, "Object.rotor_angle");
SR_STATIC_ASSERT(offsetof(Object, num_seats) == 0x81a8, "Object.num_seats");

/* connection[]: registro de 0xf170 bytes. */
typedef struct Connection {
    uint32_t ip;                         /* +0x0000 manager (ANALISE-NPCS.md) */
    int port;                            /* +0x0004 manager (ANALISE-NPCS.md) */
    int unk_0008;                        /* +0x0008 2 funções */
    int unk_0c;                          /* +0x000c packevent: == 0 libera eventos de tipo 2 */
    int player_id;                       /* +0x0010 manager (ANALISE-NPCS.md) */
    int send_timer;                      /* +0x0014 serversend: < 1 envia e recarrega (provável) */
    int timeout;                         /* +0x0018 servercycle: dividido por 0x3c (60) (provável) */
    int packets_received;                /* +0x001c serverrecv incrementa (provável) */
    uint8_t pad_0020[0x24];
    int event_ack;                       /* +0x0044 serverrecv lê 16 bits do cliente; serversend envia eventos de event_ack até numofevents (máx. 31 por pacote */
    uint8_t pad_0048[0x4];
    uint32_t sequence;                   /* +0x004c packobjectpacket2: compara bits &3 e &0x1f (provável) */
    uint8_t pad_0050[0x104];
    int unk_0154;                        /* +0x0154 2 funções */
    uint8_t pad_0158[0xf000];
    vec3 camera_pos;                     /* +0xf158 packobjectpacket: copyvector para distância */
    vec3 listener_pos;                   /* +0xf164 packevent: vectordistance para alcance de som */
} Connection;
SR_STATIC_ASSERT(sizeof(Connection) == 0xf170, "Connection");
SR_STATIC_ASSERT(offsetof(Connection, ip) == 0x0, "Connection.ip");
SR_STATIC_ASSERT(offsetof(Connection, port) == 0x4, "Connection.port");
SR_STATIC_ASSERT(offsetof(Connection, unk_0c) == 0xc, "Connection.unk_0c");
SR_STATIC_ASSERT(offsetof(Connection, player_id) == 0x10, "Connection.player_id");
SR_STATIC_ASSERT(offsetof(Connection, send_timer) == 0x14, "Connection.send_timer");
SR_STATIC_ASSERT(offsetof(Connection, timeout) == 0x18, "Connection.timeout");
SR_STATIC_ASSERT(offsetof(Connection, packets_received) == 0x1c, "Connection.packets_received");
SR_STATIC_ASSERT(offsetof(Connection, event_ack) == 0x44, "Connection.event_ack");
SR_STATIC_ASSERT(offsetof(Connection, sequence) == 0x4c, "Connection.sequence");
SR_STATIC_ASSERT(offsetof(Connection, camera_pos) == 0xf158, "Connection.camera_pos");
SR_STATIC_ASSERT(offsetof(Connection, listener_pos) == 0xf164, "Connection.listener_pos");

/* itemtype[]: registro de 0x198 bytes. */
typedef struct ItemType {
    uint8_t pad_0000[0x4];
    int unk_0004;                        /* +0x0004 1 funções */
    int unk_0008;                        /* +0x0008 1 funções */
    uint8_t pad_000c[0x4];
    int unk_0010;                        /* +0x0010 1 funções */
    int unk_0014;                        /* +0x0014 3 funções */
    int unk_0018;                        /* +0x0018 1 funções */
    int unk_001c;                        /* +0x001c 1 funções */
    float unk_0020;                      /* +0x0020 1 funções */
    int unk_0024;                        /* +0x0024 1 funções */
    uint8_t pad_0028[0x28];
    int unk_0050;                        /* +0x0050 1 funções */
    uint8_t pad_0054[0x18];
    float unk_006c;                      /* +0x006c 1 funções */
    int unk_0070;                        /* +0x0070 1 funções */
    float unk_0074;                      /* +0x0074 1 funções */
    float unk_0078;                      /* +0x0078 1 funções */
    int unk_007c;                        /* +0x007c 1 funções */
    vec3 size;                           /* +0x0080 setupitemtypes: produtos de 0x80/0x84/0x88 (provável: dimensões para massa) */
    float unk_008c;                      /* +0x008c 3 funções */
    float unk_0090;                      /* +0x0090 2 funções */
    float unk_0094;                      /* +0x0094 2 funções */
    uint8_t pad_0098[0x100];
} ItemType;
SR_STATIC_ASSERT(sizeof(ItemType) == 0x198, "ItemType");
SR_STATIC_ASSERT(offsetof(ItemType, size) == 0x80, "ItemType.size");

/* objecttype[]: registro de 0x828c bytes. */
typedef struct ObjectType {
    uint8_t pad_0000[0xc];
    int unk_000c;                        /* +0x000c 1 funções */
    uint8_t pad_0010[0x1c];
    int unk_002c;                        /* +0x002c 4 funções */
    float unk_0030;                      /* +0x0030 4 funções */
    float unk_0034;                      /* +0x0034 1 funções */
    int num_particles;                   /* +0x0038 objecttype_addparticle incrementa (passo 0x14) */
    uint8_t pad_003c[0x7c];
    int unk_00b8;                        /* +0x00b8 1 funções */
    uint8_t pad_00bc[0x24];
    int unk_00e0;                        /* +0x00e0 1 funções */
    uint8_t pad_00e4[0x38];
    int unk_011c;                        /* +0x011c 1 funções */
    uint8_t pad_0120[0x41c];
    int unk_053c;                        /* +0x053c 5 funções */
    int unk_0540;                        /* +0x0540 1 funções */
    uint8_t pad_0544[0xfc];
    int unk_0640;                        /* +0x0640 1 funções */
    uint8_t pad_0644[0xfc];
    int num_bonds;                       /* +0x0740 objecttype_addbond incrementa (passo 0xc) */
    uint8_t pad_0744[0xc00];
    int num_faces;                       /* +0x1344 objecttype_addface incrementa (passo 0x40) */
    uint8_t pad_1348[0x4000];
    int num_quads;                       /* +0x5348 setupobjectypes_train: 4 índices de partícula por registro (passo 0x20); objectsimulation (provável) */
    uint8_t pad_534c[0x2000];
    int unk_734c;                        /* +0x734c 1 funções */
    uint8_t pad_7350[0x408];
    float engine_power;                  /* +0x7758 createobject copia (× constante) para object.engine_power */
    int num_wheels;                      /* +0x775c objecttype_addwheel incrementa (passo 0x44) */
    uint8_t pad_7760[0x880];
    float unk_7fe0;                      /* +0x7fe0 2 funções */
    float unk_7fe4;                      /* +0x7fe4 2 funções */
    uint8_t pad_7fe8[0x94];
    int num_seats;                       /* +0x807c setupobjecttypes_*: lista de vec3 em +0x8080; createobject copia para object+0x81ac (provável: assentos) */
    uint8_t pad_8080[0x208];
    int unk_8288;                        /* +0x8288 1 funções */
} ObjectType;
SR_STATIC_ASSERT(sizeof(ObjectType) == 0x828c, "ObjectType");
SR_STATIC_ASSERT(offsetof(ObjectType, num_particles) == 0x38, "ObjectType.num_particles");
SR_STATIC_ASSERT(offsetof(ObjectType, num_bonds) == 0x740, "ObjectType.num_bonds");
SR_STATIC_ASSERT(offsetof(ObjectType, num_faces) == 0x1344, "ObjectType.num_faces");
SR_STATIC_ASSERT(offsetof(ObjectType, num_quads) == 0x5348, "ObjectType.num_quads");
SR_STATIC_ASSERT(offsetof(ObjectType, engine_power) == 0x7758, "ObjectType.engine_power");
SR_STATIC_ASSERT(offsetof(ObjectType, num_wheels) == 0x775c, "ObjectType.num_wheels");
SR_STATIC_ASSERT(offsetof(ObjectType, num_seats) == 0x807c, "ObjectType.num_seats");

/* bond[]: registro de 0xec bytes. */
typedef struct Bond {
    int state;                           /* +0x0000 createbond grava 0 */
    int type;                            /* +0x0004 createbond: parâmetro 1 */
    int unk_0008;                        /* +0x0008 1 funções */
    int mode;                            /* +0x000c createbond: 3 */
    int unk_10;                          /* +0x0010 createbond grava 2 */
    int particle_a;                      /* +0x0014 createbond: parâmetro 2 (índice em particle[]) */
    int particle_b;                      /* +0x0018 createbond: parâmetro 3 */
    int unk_001c;                        /* +0x001c 1 funções */
    int unk_0020;                        /* +0x0020 1 funções */
    uint8_t pad_0024[0x4];
    int unk_0028;                        /* +0x0028 1 funções */
    uint8_t pad_002c[0x8];
    vec3 local_a;                        /* +0x0034 bondsimulation2: ponto no osso (pesos das 3 linhas da rotação do Bone) */
    vec3 local_b;                        /* +0x0040 bondsimulation2 tipo 10: ponto no item (pesos das linhas de item.rot) */
    float mass_ratio[2];                 /* +0x004c bondsimulation2 tipo 10: massa_item/(soma) e massa_osso/(soma) */
    float unk_0054;                      /* +0x0054 1 funções */
    uint8_t pad_0058[0x8];
    float rest_length;                   /* +0x0060 bondsimulation2: (dist - rest) * k (provável) */
    uint8_t pad_0064[0x8];
    float stiffness;                     /* +0x006c bondsimulation2: força = (distância - rest_length) * stiffness */
    float unk_70;                        /* +0x0070 bondsimulation2 copia para bondopt+0x3c e usa negado */
    vec3 friction_dir;                   /* +0x0074 bondsimulation2: scalevector e calculatefriction (provável) */
    int unk_0080;                        /* +0x0080 1 funções */
    uint8_t pad_0084[0x8];
    int unk_008c;                        /* +0x008c 1 funções */
    int human_id;                        /* +0x0090 bondsimulation2 tipos 8 e 10: índice em human[] */
    int bone_a;                          /* +0x0094 bondsimulation2: índice do osso (passo 0xe0) */
    int target;                          /* +0x0098 bondsimulation2: tipo 10 = índice do item; tipo 8 = segundo osso */
    int unk_009c;                        /* +0x009c 1 funções */
    uint8_t pad_00a0[0x10];
    int unk_00b0;                        /* +0x00b0 1 funções */
    uint8_t pad_00b4[0x8];
    int unk_00bc;                        /* +0x00bc 1 funções */
    uint8_t pad_00c0[0x2c];
} Bond;
SR_STATIC_ASSERT(sizeof(Bond) == 0xec, "Bond");
SR_STATIC_ASSERT(offsetof(Bond, state) == 0x0, "Bond.state");
SR_STATIC_ASSERT(offsetof(Bond, type) == 0x4, "Bond.type");
SR_STATIC_ASSERT(offsetof(Bond, mode) == 0xc, "Bond.mode");
SR_STATIC_ASSERT(offsetof(Bond, unk_10) == 0x10, "Bond.unk_10");
SR_STATIC_ASSERT(offsetof(Bond, particle_a) == 0x14, "Bond.particle_a");
SR_STATIC_ASSERT(offsetof(Bond, particle_b) == 0x18, "Bond.particle_b");
SR_STATIC_ASSERT(offsetof(Bond, local_a) == 0x34, "Bond.local_a");
SR_STATIC_ASSERT(offsetof(Bond, local_b) == 0x40, "Bond.local_b");
SR_STATIC_ASSERT(offsetof(Bond, mass_ratio) == 0x4c, "Bond.mass_ratio");
SR_STATIC_ASSERT(offsetof(Bond, rest_length) == 0x60, "Bond.rest_length");
SR_STATIC_ASSERT(offsetof(Bond, stiffness) == 0x6c, "Bond.stiffness");
SR_STATIC_ASSERT(offsetof(Bond, unk_70) == 0x70, "Bond.unk_70");
SR_STATIC_ASSERT(offsetof(Bond, friction_dir) == 0x74, "Bond.friction_dir");
SR_STATIC_ASSERT(offsetof(Bond, human_id) == 0x90, "Bond.human_id");
SR_STATIC_ASSERT(offsetof(Bond, bone_a) == 0x94, "Bond.bone_a");
SR_STATIC_ASSERT(offsetof(Bond, target) == 0x98, "Bond.target");

/* event[]: registro de 0x80 bytes. */
typedef struct Event {
    int type;                            /* +0x0000 packevent: pbitwrite 6 bits; createevent_sound grava 9 */
    int tick;                            /* +0x0004 packevent: 28 bits; comparado com game+0x34 (idade < 600) */
    vec3 pos;                            /* +0x0008 packevent: cs_pwrite 3 floats; createevent_sound: posição */
    vec3 vec;                            /* +0x0014 packevent: cs_pwrite 3 floats */
    int param;                           /* +0x0020 createevent_sound: id do som; packevent: 4 bits em alguns tipos */
    int attach_type;                     /* +0x0024 packevent: 1 = human (usa human+0x288) */
    int attach_id;                       /* +0x0028 packevent: índice do human/item; 10 bits com sinal */
    int range_mode;                      /* +0x002c packevent: 0 reduz */
    float sound_a;                       /* +0x0030 createevent_sound: parâmetro 3 */
    float sound_b;                       /* +0x0034 createevent_sound: parâmetro 4 */
    uint8_t pad_0038[0x8];
    char message[64];                    /* +0x0040 createevent_message: memcpy do texto (máx. 63 caracteres) */
} Event;
SR_STATIC_ASSERT(sizeof(Event) == 0x80, "Event");
SR_STATIC_ASSERT(offsetof(Event, type) == 0x0, "Event.type");
SR_STATIC_ASSERT(offsetof(Event, tick) == 0x4, "Event.tick");
SR_STATIC_ASSERT(offsetof(Event, pos) == 0x8, "Event.pos");
SR_STATIC_ASSERT(offsetof(Event, vec) == 0x14, "Event.vec");
SR_STATIC_ASSERT(offsetof(Event, param) == 0x20, "Event.param");
SR_STATIC_ASSERT(offsetof(Event, attach_type) == 0x24, "Event.attach_type");
SR_STATIC_ASSERT(offsetof(Event, attach_id) == 0x28, "Event.attach_id");
SR_STATIC_ASSERT(offsetof(Event, range_mode) == 0x2c, "Event.range_mode");
SR_STATIC_ASSERT(offsetof(Event, sound_a) == 0x30, "Event.sound_a");
SR_STATIC_ASSERT(offsetof(Event, sound_b) == 0x34, "Event.sound_b");
SR_STATIC_ASSERT(offsetof(Event, message) == 0x40, "Event.message");

/* bullet[]: registro de 0x5c bytes. */
typedef struct Bullet {
    int type;                            /* +0x0000 createbullet: parâmetro 1 */
    int ttl;                             /* +0x0004 createbullet grava 600 */
    int owner;                           /* +0x0008 createbullet: parâmetro 4 */
    float unk_000c;                      /* +0x000c 1 funções */
    float unk_0010;                      /* +0x0010 1 funções */
    vec3 pos;                            /* +0x0014 createbullet: parâmetro 2 */
    vec3 pos_prev;                       /* +0x0020 createbullet: cópia da posição */
    vec3 vel;                            /* +0x002c createbullet: parâmetro 3 */
    int unk_0038;                        /* +0x0038 1 funções */
    uint8_t pad_003c[0x20];
} Bullet;
SR_STATIC_ASSERT(sizeof(Bullet) == 0x5c, "Bullet");
SR_STATIC_ASSERT(offsetof(Bullet, type) == 0x0, "Bullet.type");
SR_STATIC_ASSERT(offsetof(Bullet, ttl) == 0x4, "Bullet.ttl");
SR_STATIC_ASSERT(offsetof(Bullet, owner) == 0x8, "Bullet.owner");
SR_STATIC_ASSERT(offsetof(Bullet, pos) == 0x14, "Bullet.pos");
SR_STATIC_ASSERT(offsetof(Bullet, pos_prev) == 0x20, "Bullet.pos_prev");
SR_STATIC_ASSERT(offsetof(Bullet, vel) == 0x2c, "Bullet.vel");

/* body[]: registro de 0x40 bytes. */
typedef struct Body {
    uint8_t pad_0000[0x4];
    vec3 pos;                            /* +0x0004 human_calculatearmangles: subtractvectors com a posição do osso */
    int unk_0010;                        /* +0x0010 3 funções */
    int unk_0014;                        /* +0x0014 4 funções */
    int unk_0018;                        /* +0x0018 4 funções */
    float unk_001c;                      /* +0x001c 2 funções */
    float unk_0020;                      /* +0x0020 3 funções */
    float unk_0024;                      /* +0x0024 3 funções */
    float unk_0028;                      /* +0x0028 3 funções */
    int unk_002c;                        /* +0x002c 1 funções */
    float unk_0030;                      /* +0x0030 2 funções */
    float unk_0034;                      /* +0x0034 1 funções */
    float unk_0038;                      /* +0x0038 1 funções */
    uint8_t pad_003c[0x4];
} Body;
SR_STATIC_ASSERT(sizeof(Body) == 0x40, "Body");
SR_STATIC_ASSERT(offsetof(Body, pos) == 0x4, "Body.pos");

#pragma pack(pop)

#endif /* SR_TYPES_H */
