/* The network of the server: the cycle of the connections, the receive side, the sockets and
 * the voice. See docs/game/network.md.
 *
 * The functions read the packet fields in the order of the wire format (docs/protocol.md). A
 * read that fails leaves its target as it was. Some locals keep an old value on purpose (the
 * type of the last packet, the voice frame, the number of actions), so a short packet reuses it.
 *
 * The floats of a player input and the vectors of a camera are copied as raw bytes (cs_pread),
 * so no float load changes their bits.
 */
#include "server.h"

#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "packet.h"
#include "player.h"
#include "profiler.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_server.h"
#include "vectors.h"

#define VOICE_SLOTS 8
#define IDLE_LIMIT 0x4b0 /* ticks without a packet before a connection drops */

/* Add the voice slot for a speaker at distance dist, or find the slot to replace. Returns the
 * index of a free slot. When all 8 are busy, only the first slot can be replaced, and only
 * when the speaker is closer than its speaker. The test also reads the slot before slot 0
 * (voice_key): at 65536 or more no slot is replaced. */
int addvoice(int conn, float dist)
{
    Connection *c = &sr_connections[conn];
    int i;
    for (i = 0; i < VOICE_SLOTS; i++) {
        if (!c->voice[i].active)
            return i;
    }
    if (c->voice_key < 65536.0f && dist < c->voice[0].distance)
        return 0;
    return -1;
}

/* The distance to a point, as the sum of squares in float and a square root. */
static float distance_to(const vec3 *p, const vec3 *l)
{
    float dx = (float)(p->x - l->x), dy = (float)(p->y - l->y), dz = (float)(p->z - l->z);
    float sq = (float)(dz * dz + (dx * dx + dy * dy));
    return (float)x87_sqrt(sq);
}

/* A radio is heard 4 times as far in the distance test, so its distance counts 4 times. */
static float radio_distance(const vec3 *p, const vec3 *l)
{
    return (float)(distance_to(p, l) * 4.0);
}

/* Set the slot for a speaker. */
static void set_voice(VoiceSlot *s, float dist, int player, int human, int item)
{
    s->distance = dist;
    s->active = 1;
    s->player = player;
    s->human = human;
    s->item = item;
}

/* Choose who the player of a connection hears: the 8 voice slots. A slot holds a speaker that
 * is near (a direct voice) or a radio of a linked phone pair. Loop 1 checks the slots that are
 * in use. Loop 2 adds the players that are near. Loop 3 adds the radios. */
void calculateplayervoice(int conn, int player)
{
    Connection *c = &sr_connections[conn];
    VoiceSlot *slots = c->voice;
    VoiceRing *rings = sr_speex.voice_ring;
    Player *players = sr_players;
    int lh, view, i, q;
    vec3 listener;

    if (!sr_option.voicechat) {
        for (i = 0; i < VOICE_SLOTS; i++)
            slots[i].active = 0;
        return;
    }
    lh = players[player].human_id;
    view = lh;
    if (lh == -1 && (view = c->spectate) == -1)
        view = 0;
    listener = sr_humans[view].bones[2].pos;

    for (i = 0; i < VOICE_SLOTS; i++) {
        VoiceSlot *s = &slots[i];
        int p;
        if (!s->active)
            continue;
        p = s->player;
        if (!players[p].active)
            s->active = 0;
        if (s->item == -1) {
            int h = players[p].human_id;
            s->human = h;
            if (rings[p].voice_off)
                s->active = 0;
            if (lh != -1 && h == -1)
                s->active = 0;
            if (h != -1)
                s->distance = vectordistance(&listener, &sr_humans[h].bones[2].pos);
            if (256.0f < s->distance)
                s->active = 0;
        } else {
            const Item *radio = &sr_items[s->item];
            int li = radio->linked_item;
            float d;
            if (rings[p].voice_off)
                s->active = 0;
            if (li == -1) {
                s->active = 0;
            } else {
                const Item *other = &sr_items[li];
                if (other->parent_human == -1)
                    s->active = 0;
                if (other->parent_slot != 0)
                    s->active = 0;
                if (other->linked_item == -1)
                    s->active = 0;
                if (other->call_state != 3)
                    s->active = 0;
            }
            d = radio_distance(&radio->pos, &listener);
            s->distance = d;
            if (256.0f < d)
                s->active = 0;
        }
    }

    for (q = 0; q < SR_MAX_PLAYER; q++) {
        int h, k, found = 0;
        float d;
        if (q == player || !players[q].active || rings[q].voice_off)
            continue;
        h = players[q].human_id;
        for (k = 0; k < VOICE_SLOTS; k++) {
            if (slots[k].active && slots[k].player == q && slots[k].human == h) {
                found = 1;
                break;
            }
        }
        if (found)
            continue;
        if (lh != -1 && h == -1)
            continue;
        if (h == -1) {
            d = 8.0f;
        } else {
            d = distance_to(&sr_humans[h].bones[2].pos, &listener);
            if (!(d < 256.0f))
                continue;
        }
        k = addvoice(conn, d);
        if (k != -1)
            set_voice(&slots[k], d, q, h, -1);
    }

    for (i = 0; i < SR_MAX_ITEM; i++) {
        const Item *it = &sr_items[i];
        int linked, ph, k, found = 0;
        float d;
        if (!it->active || it->type != 0x17)
            continue;
        linked = it->linked_item;
        ph = it->parent_human;
        if (linked == -1 || ph == -1 || it->parent_slot != 0 || it->call_state != 3)
            continue;
        for (k = 0; k < VOICE_SLOTS; k++) {
            if (slots[k].active && slots[k].player == sr_humans[ph].player_id && slots[k].item == linked) {
                found = 1;
                break;
            }
        }
        if (found || sr_items[linked].linked_item != i)
            continue;
        d = radio_distance(&sr_items[linked].pos, &listener);
        if (d < 256.0f && (k = addvoice(conn, d)) != -1)
            set_voice(&slots[k], d, sr_humans[ph].player_id, -1, linked);
    }
}

/* Each tick: send the heartbeat to the master server (every 1024 ticks), give each connection
 * its share of the bandwidth, and let its credit go down by one packet size in 60. */
void servercycle(void)
{
    BsdSocket *bs = &sr_bsdsocket;
    int n = sr_numofconnections, i;
    if (!sr_option.net_on)
        return;
    if (sr_option.master_on && (sr_game.ticks & 0x3ff) == 0) {
        uint8_t kind = 0x20;
        bs->send_bit = 0;
        memcpy(bs->send_buf, &sr_socketid, 4);
        bs->send_size = 4;
        cs_pwrite(&kind, 1, 1);
        sendpacket(sr_masterserver.ip, (uint16_t)sr_masterserver.port);
    }
    n = sr_numofconnections;
    if (n > 0) {
        int rate = sr_option.bandwidth / n;
        for (i = 0; i < n; i++)
            sr_connections[i].rate = rate;
        for (i = 0; i < n; i++) {
            Connection *c = &sr_connections[i];
            int step = c->rate / -60;
            c->credit += step;
            if (c->credit < step)
                c->credit = step;
        }
    }
}

/* Open the UDP socket with the number index, on the port of the options, without blocking. */
void disablesocket(int index)
{
    BsdSocket *bs = &sr_bsdsocket;
    uint16_t family = 2, port;
    uint32_t any = 0;
    bs->port = (int16_t)sr_option.port;
    bs->handle[index] = (int)os_socket(2, 2, 17);
    port = _byteswap_ushort((uint16_t)bs->port);
    memcpy(&bs->addr[0], &family, 2);
    memcpy(&bs->addr[2], &port, 2);
    memcpy(&bs->addr[4], &any, 4);
    bs->addr_len = 16;
    if (os_bind((uintptr_t)(unsigned)bs->handle[index], bs->addr, 16) == 0) {
        unsigned long one = 1;
        os_ioctlsocket((uintptr_t)(unsigned)bs->handle[index], (long)0x8004667e, &one);
        bs->enabled[index] = 1;
    }
}

/* The address of the last datagram: the ip and the port in host order. */
static uint32_t sender_ip(void)
{
    uint32_t v;
    memcpy(&v, &sr_bsdsocket.addr[4], 4);
    return _byteswap_ulong(v);
}

static uint16_t sender_port(void)
{
    uint16_t v;
    memcpy(&v, &sr_bsdsocket.addr[2], 2);
    return _byteswap_ushort(v);
}

/* The connection of the sender, or -1. The first match when first is set, else the last. */
static int find_connection(int first)
{
    uint32_t ip = sender_ip();
    uint16_t port = sender_port();
    int n = sr_numofconnections, i, found = -1;
    for (i = 0; i < n; i++) {
        const Connection *c = &sr_connections[i];
        if (c->ip == ip && (uint16_t)c->port == port) {
            if (first)
                return i;
            found = i;
        }
    }
    return found;
}

/* A line of the log of the server, as a chat message. */
static void announce(const Player *p, const char *suffix)
{
    char msg[64];
    strcpy(msg, p->name);
    strcat(msg, suffix);
    sr_createevent_message(0, msg, -1, 0);
}

/* Type 0: a server query. The reply has the version, the game type, the player counts and the
 * name. Clients of version 7 or newer also get the account key. */
static void reply_query(uint32_t ver, uint32_t nonce, uint8_t *type)
{
    BsdSocket *bs = &sr_bsdsocket;
    bs->send_bit = 0;
    memcpy(bs->send_buf, &sr_socketid, 4);
    bs->send_size = 4;
    *type = 1;
    cs_pwrite(type, 1, 1);
    pbitwrite((const uint32_t *)&sr_currentversion, 8);
    cs_pwrite(&nonce, 4, 1);
    pbitwrite((const uint32_t *)&sr_game.gametype, 4);
    pbitwrite((const uint32_t *)&sr_serverinfo.num_players, 8);
    pbitwrite((const uint32_t *)&sr_serverinfo.max_players, 8);
    cs_pwrite(sr_serverinfo.name, 32, 1);
    if (6 < (int)ver)
        cs_pwrite(&sr_account_key, 4, 1);
    sendpacket(sender_ip(), sender_port());
}

/* The human that belongs to the account of a new player takes the player over. */
static void adopt_humans(int player, int account)
{
    Player *pl = &sr_players[player];
    int h;
    for (h = 0; h < SR_MAX_HUMAN; h++) {
        Human *hm = &sr_humans[h];
        if (hm->active != 0 && hm->account == account) {
            hm->player_id = player;
            pl->human_id = h;
            if (sr_game.gametype == 4) {
                pl->team = 7;
                hm->team = 7;
            } else {
                pl->team = hm->team;
                pl->menu = 0;
            }
            createevent_updatehuman(h, player);
        }
    }
}

/* Add the connection of the sender for a player. */
static void add_connection(int player)
{
    Connection *c = &sr_connections[sr_numofconnections];
    c->ip = sender_ip();
    *(uint16_t *)&c->port = sender_port();
    c->player_id = player;
    c->idle_ticks = 0;
    sr_numofconnections++;
    c->event_ack = 0;
    c->game_id = -1;
    c->sequence = 0xffffffffu;
    c->is_admin = 0;
}

/* Type 2: a join. The player gets a slot, the name and the account id from the packet, and
 * its account. A player of the same account that is still in the game is replaced. */
static void handle_join(uint32_t *ver)
{
    Player *players = sr_players;
    int p, i, a, n;
    if (!pbitread(ver, 8) || (int)*ver != sr_currentversion)
        return;
    i = find_connection(1);
    if (i != -1) {
        sr_connections[i].idle_ticks = 0;
        return;
    }
    if (sr_serverinfo.num_players >= sr_serverinfo.max_players)
        return;
    memset(&sr_connections[sr_numofconnections], 0, sizeof(Connection));
    p = createplayer();
    if (p == -1)
        return;
    cs_pread(players[p].name, 32, 1);
    cs_pread(&players[p].account_id[0], 4, 1);
    cs_pread(&players[p].account_id[1], 4, 1);
    sr_fixplayername(p);
    a = createaccount(p);
    players[p].account = a;
    n = sr_numofconnections;
    for (i = 0; i < n; i++) {
        int other = sr_connections[i].player_id;
        if (other != -1 && players[other].account == a) {
            deleteplayer(p);
            p = other;
            sr_connections[i].player_id = -1;
            add_connection(p);
            return;
        }
    }
    if (a != -1) {
        const Account *acc = &sr_accounts[a];
        players[p].money = acc->money;
        players[p].look_a = acc->look_a;
        players[p].look_b = acc->look_b;
        players[p].look_c = acc->look_c;
        if (acc->ban_time > 0) {
            deleteplayer(p);
            return;
        }
        adopt_humans(p, a);
    }
    if (sr_debugit != 0 && players[p].money < 5000000)
        players[p].money = 5000000;
    if (players[p].money < 0)
        players[p].money = 0;
    sr_serverinfo.num_players++;
    createevent_updateplayer(p);
    createevent_updateplayer_finance(p);
    announce(&players[p], " Joined");
    add_connection(p);
}

/* The state that one type 4 packet leaves in locals between reads. */
typedef struct InputState {
    uint32_t frame; /* the voice frame, and the counter of the chat text before it */
    uint32_t count; /* the number of actions */
    uint32_t spec;  /* the spectated human */
} InputState;

/* Read the actions of a player: a menu choice, a chat text or an item action. The ring has 64
 * records but the first index is 8 bits, so a client can write up to 255 records. */
static void read_actions(Player *pl, InputState *st)
{
    uint32_t left = st->count;
    if ((int)left <= 0)
        return;
    do {
        PlayerAction *a = &pl->actions[pl->num_actions];
        pbitread((uint32_t *)&a->kind, 4);
        if (a->kind == 0) {
            pbitread((uint32_t *)&a->arg_a, 8);
            cs_pread(&a->arg_b, 4, 1);
        }
        if (a->kind == 1) {
            pbitread((uint32_t *)&a->arg_a, 6);
            st->frame = 0;
            while ((int)st->frame < a->arg_a) {
                cs_pread(&a->text[st->frame], 1, 1);
                st->frame++;
            }
            a->text[a->arg_a] = 0;
            pbitread((uint32_t *)&a->arg_b, 4);
        }
        if (a->kind == 2) {
            pbitread((uint32_t *)&a->arg_a, 16);
            pbitread((uint32_t *)&a->arg_b, 16);
        }
        left--;
        pl->num_actions = (int)((uint32_t)(pl->num_actions + 1) & 0x3f);
    } while (left != 0);
}

/* Type 4: the input of a player. The fields are in the order of the wire format. The voice
 * frames come last: 6 of them, each into the ring of the player. The index of a frame is 8
 * bits and is not masked (hook/netguard.c masks it before the game reads the packet). */
static void handle_input(InputState *st)
{
    int i = find_connection(0), k;
    Connection *c;
    Player *pl;
    VoiceRing *ring;
    if (i == -1)
        return;
    c = &sr_connections[i];
    pl = &sr_players[c->player_id];
    ring = &sr_speex.voice_ring[c->player_id];
    c->idle_ticks = 0;
    cs_pread(&c->game_id, 4, 1);
    if (c->game_id != sr_game.round_number)
        return;
    for (k = 0; k < 4; k++)
        cs_pread(&pl->input_axes[k], 4, 1);
    cs_pread(&pl->yaw, 4, 1);
    cs_pread(&pl->input_pitch, 4, 1);
    cs_pread(&pl->input_extra[0], 4, 1);
    cs_pread(&pl->input_extra[1], 4, 1);
    cs_pread(&pl->input_buttons, 4, 1);
    pbitread((uint32_t *)&pl->input_mode, 8);
    pbitread((uint32_t *)&pl->move_mode, 4);
    pbitread((uint32_t *)&c->event_ack, 16);
    cs_pread(&c->camera_pos, 4, 3);
    pbitread(&st->count, 4);
    pbitread((uint32_t *)&pl->num_actions, 8);
    read_actions(pl, st);
    for (k = 0; k < 6; k++) {
        pbitread(&st->frame, 8);
        pbitread((uint32_t *)&ring->size[st->frame], 8);
        pbitread((uint32_t *)&ring->mode[st->frame], 2);
        cs_pread(ring->data[st->frame], ring->size[st->frame], 1);
    }
    ring->head = (int)((st->frame + 1) & 0x3f);
    pbitread((uint32_t *)&ring->voice_off, 1);
    pbitread(&st->spec, 8);
    c->spectate = st->spec == 0xff ? -1 : (int)st->spec;
}

/* Drop the connections that were idle too long, or that belong to a banned account, and send
 * nothing to them. The last connection moves into the free place. */
static void drop_connections(void)
{
    Player *players = sr_players;
    int n = sr_numofconnections, i = 0;
    while (i < n) {
        Connection *c = &sr_connections[i];
        int p = c->player_id, a;
        if (p == -1 || ((a = players[p].account) >= 0 && a < sr_numofaccounts && sr_accounts[a].ban_time > 0))
            c->idle_ticks = IDLE_LIMIT;
        if (p != -1 && players[p].account == -1)
            c->idle_ticks = IDLE_LIMIT;
        if (c->idle_ticks < IDLE_LIMIT) {
            i++;
            continue;
        }
        if (p != -1) {
            if (players[p].account != -1)
                sr_accounts[players[p].account].name_locked = 2;
            deleteplayer(p);
            announce(&players[p], " Exited");
        }
        n--;
        memcpy(c, &sr_connections[n], sizeof(Connection));
    }
    sr_numofconnections = n;
}

/* Each tick: count the players, drop the idle connections and handle each packet that came. */
void serverrecv(void)
{
    uint8_t type = 0xff; /* the type byte of the last packet: a short packet keeps the old one */
    uint32_t ver = 0, nonce = 0;
    InputState st = { 0, 0, 0 };
    int count = 0, n, i;

    for (i = 0; i < SR_MAX_PLAYER; i++) {
        if (sr_players[i].active != 0 && sr_players[i].is_bot == 0)
            count++;
    }
    sr_serverinfo.num_players = count;
    n = sr_numofconnections;
    for (i = 0; i < n; i++)
        sr_connections[i].idle_ticks++;
    drop_connections();

    for (n = recvpacket(); n >= 1; n = recvpacket()) {
        uint32_t ip;
        sr_serverinfo.packets_in++;
        ip = sender_ip();
        for (i = 0; i < sr_numofkicked; i++) {
            if (sr_kickedip[i] == ip)
                break;
        }
        if (i < sr_numofkicked)
            continue;
        cs_pread(&type, 1, 1);
        if (type == 0) {
            pbitread(&ver, 8);
            cs_pread(&nonce, 4, 1);
            reply_query(ver, nonce, &type);
        } else if (type == 2) {
            handle_join(&ver);
        } else if (type == 4) {
            handle_input(&st);
        } else if (type == 7) {
            i = find_connection(0);
            if (i != -1)
                sr_connections[i].idle_ticks = 3000;
        }
    }
}

/* The packet that tells a client the game changed: the round number, the game type and the
 * two start angles. */
static void send_new_game(Connection *c)
{
    BsdSocket *bs = &sr_bsdsocket;
    uint8_t kind = 6;
    memcpy(bs->send_buf, &sr_socketid, 4);
    bs->send_buf[4] = kind;
    memcpy(&bs->send_buf[5], &sr_game.round_number, 4);
    bs->send_bit = 0;
    bs->send_size = 9;
    pbitwrite((const uint32_t *)&sr_game.gametype, 4);
    pbitwrite((const uint32_t *)&sr_game.round_mode, 4);
    packangle(&sr_game.start_angle_a, 16);
    packangle(&sr_game.start_angle_b, 16);
    sr_serverinfo.last_packet_size = bs->send_size;
    c->credit += bs->send_size;
    sendpacket(c->ip, (uint16_t)c->port);
}

/* The rows of the shop of the building that a player looks at (menu 10). */
static void write_shop(int building)
{
    const int *shop = (const int *)((const uint8_t *)&sr_city.shop_count + building * 0x9dc);
    int count = shop[0], k;
    pbitwrite((const uint32_t *)&shop[0], 8);
    for (k = 0; k < count; k++) {
        pbitwrite((const uint32_t *)&shop[1 + 3 * k], 8);
        pbitwrite((const uint32_t *)&shop[2 + 3 * k], 24);
        pbitwrite((const uint32_t *)&shop[3 + 3 * k], 4);
    }
}

/* The inventory that the client shows: 7 slots, each with a count and the item numbers. */
static void write_hud_inventory(const Player *pl)
{
    int s, j;
    for (s = 0; s < 7; s++) {
        const int *slot = &pl->hud_inventory[s].count;
        pbitwrite((const uint32_t *)slot, 4);
        for (j = 1; j <= slot[0]; j++)
            pbitwrite((const uint32_t *)&slot[j], 16);
    }
}

/* The type 5 packet: the state of the game, the player and the events that the client did not
 * confirm. The number of events is written again when all events are in, so the count fits. */
static void send_state(Connection *c, int conn)
{
    BsdSocket *bs = &sr_bsdsocket;
    int p = c->player_id, k, hid, ack, count, size, bit, end_size, end_bit;
    Player *pl = &sr_players[p];
    vec3 vel = { 0.0f, 0.0f, 0.0f };
    if (p == -1 || (hid = pl->human_id) == -1 || sr_humans[hid].health < -4)
        c->listener_pos = c->camera_pos;
    else
        c->listener_pos = sr_humans[hid].pos;
    memcpy(bs->send_buf, &sr_socketid, 4);
    bs->send_buf[4] = 5;
    memcpy(&bs->send_buf[5], &sr_game.round_number, 4);
    bs->send_bit = 0;
    memcpy(&bs->send_buf[9], &sr_game.ticks, 4);
    bs->send_size = 13;
    pbitwrite((const uint32_t *)&sr_game.state, 4);
    if (sr_game.state == 1) {
        for (k = 0; k < 32; k++)
            pbitwrite((const uint32_t *)&sr_players[k].ready, 1);
    }
    pbitwrite((const uint32_t *)&sr_game.timer, 16);
    pbitwrite((const uint32_t *)&sr_game_world.clock, 30);
    pbitwrite((const uint32_t *)&sr_game.team0_members, 6);
    pbitwrite((const uint32_t *)&sr_game.team1_members, 6);
    pbitwrite((const uint32_t *)&sr_game.team2_members, 6);
    pbitwrite((const uint32_t *)&p, 8);
    pbitwrite((const uint32_t *)&pl->human_id, 10);
    if (p != -1 && pl->human_id != -1) {
        const Human *hm = &sr_humans[pl->human_id];
        memcpy(&pl->look_yaw, &hm->axis, 4);
        memcpy(&pl->look_pitch, &hm->pitch, 4);
        vel = hm->bones[2].vel;
    }
    packangle(&pl->look_yaw, 16);
    packangle(&pl->look_pitch, 16);
    packposition(&vel);
    pbitwrite((const uint32_t *)&pl->menu, 8);
    pbitwrite((const uint32_t *)&pl->menu_building, 16);
    if (pl->menu == 10)
        write_shop(pl->menu_building);
    cs_pwrite(&pl->money, 4, 1);
    pbitwrite((const uint32_t *)&pl->num_actions, 8);
    pbitwrite((const uint32_t *)&pl->hud_health, 8);
    pbitwrite((const uint32_t *)&pl->hud_value6, 6);
    pbitwrite((const uint32_t *)&pl->hud_value8, 8);
    write_hud_inventory(pl);
    packobjectpacket(conn, p);
    pbitwrite(&sr_numofevents, 16);
    size = bs->send_size;
    bit = bs->send_bit;
    ack = c->event_ack;
    count = (int)sr_numofevents < ack ? (int)sr_numofevents - ack + 0x10000 : (int)sr_numofevents - ack;
    if (count > 31)
        count = 31;
    if (sr_numofconnections == 0)
        count = 0;
    pbitwrite((const uint32_t *)&count, 6);
    pbitwrite((const uint32_t *)&c->event_ack, 16);
    for (k = 0; k < count; k++) {
        packevent(ack, conn);
        ack = (ack + 1) & 0xffff;
    }
    end_size = bs->send_size;
    end_bit = bs->send_bit;
    bs->send_size = size;
    bs->send_bit = bit;
    pbitwrite((const uint32_t *)&count, 6);
    c->credit += end_size;
    sr_serverinfo.last_packet_size = end_size;
    bs->send_size = end_size;
    bs->send_bit = end_bit;
    sendpacket(c->ip, (uint16_t)c->port);
}

/* The type 8 packet: the voice frames of the speakers that the client hears. For each slot,
 * the last 4 frames of the speaker go out. */
static void send_voice(Connection *c, int conn)
{
    BsdSocket *bs = &sr_bsdsocket;
    int s;
    memcpy(bs->send_buf, &sr_socketid, 4);
    bs->send_buf[4] = 8;
    bs->send_bit = 0;
    bs->send_size = 5;
    calculateplayervoice(conn, c->player_id);
    for (s = 0; s < VOICE_SLOTS; s++) {
        const VoiceSlot *slot = &c->voice[s];
        pbitwrite((const uint32_t *)&slot->active, 1);
        if (slot->active != 0) {
            const VoiceRing *ring = &sr_speex.voice_ring[slot->player];
            uint32_t k = (uint32_t)(ring->head - 4) & 0x3f;
            pbitwrite((const uint32_t *)&slot->human, 8);
            pbitwrite((const uint32_t *)&slot->item, 8);
            while ((int)k != ring->head) {
                pbitwrite(&k, 6);
                pbitwrite((const uint32_t *)&ring->size[k], 8);
                pbitwrite((const uint32_t *)&ring->mode[k], 2);
                cs_pwrite(ring->data[k], ring->size[k], 1);
                k = (k + 1) & 0x3f;
            }
        }
    }
    sr_serverinfo.last_packet_size = bs->send_size;
    c->credit += bs->send_size;
    sendpacket(c->ip, (uint16_t)c->port);
}

/* Each tick: send to each connection that has used up its credit. A client in another round
 * gets the new game packet. A client in this round gets the state and then the voice. */
void serversend(void)
{
    int n = sr_numofconnections, i;
    nametimer(g_timerlist.packetbuild, "packetbuild");
    starttimer(g_timerlist.packetbuild);
    for (i = 0; i < n; i++) {
        Connection *c = &sr_connections[i];
        if (c->credit >= 1)
            continue;
        if (c->game_id != sr_game.round_number) {
            send_new_game(c);
        } else {
            send_state(c, i);
            send_voice(c, i);
        }
    }
    stoptimer(g_timerlist.packetbuild);
}
