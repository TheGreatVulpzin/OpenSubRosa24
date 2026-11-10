/* The packet that the client sends to the server each tick.
 *
 * The packet has type 4. It holds the round number, the inputs of the player, the keys, the
 * view place, the actions that the player queued (menu choices, chat lines and item moves),
 * the last frames of voice, and a few flags. The server reads it in serverrecv.
 */
#include "setup/send.h"

#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"

#define g_game sr_data_game
#define g_player sr_data_player
#define g_speex sr_data_speex
#define g_view cl_data_view_player
#define g_view_pos cl_data_audio_camera.pos

/* Game functions of other files. */
#define resetsendpacket CL_FN(0x4fe4f0, void(__cdecl *)(void))
#define sendpacket CL_FN(0x4fe520, void(__cdecl *)(uint32_t address, uint16_t port))
#define cs_pwrite CL_FN(0x4fe5b0, int(__cdecl *)(const void *data, int size, int count))
#define pbitwrite CL_FN(0x4fe6c0, int(__cdecl *)(const uint32_t *value, uint32_t bits))
#define pbitwritenego CL_FN(0x4feb60, int(__cdecl *)(const uint32_t *value, uint32_t bits))

#define PACKET_PLAYER_INPUT 4

/* The actions of a player are in a ring of 64. One packet carries up to 15 of them. */
#define ACTION_RING 0x40
#define ACTION_RING_MASK 0x3f
#define MAX_ACTIONS_PER_PACKET 0xf

/* Kinds of player action. */
enum { ACTION_MENU = 0, ACTION_CHAT = 1, ACTION_ITEM = 2 };

/* The voice ring has 64 frames. The packet carries the last 6. */
#define VOICE_RING_MASK 0x3f
#define VOICE_FRAMES_PER_PACKET 6

static void write_action(const PlayerAction *action)
{
    int i;

    pbitwrite((const uint32_t *)&action->kind, 4);
    if (action->kind == ACTION_MENU) {
        pbitwrite((const uint32_t *)&action->arg_a, 8);
        cs_pwrite(&action->arg_b, 4, 1);
    }
    if (action->kind == ACTION_CHAT) {
        pbitwrite((const uint32_t *)&action->arg_a, 6);
        for (i = 0; i < action->arg_a; i++)
            cs_pwrite(&action->text[i], 1, 1);
        pbitwrite((const uint32_t *)&action->arg_b, 4);
    }
    if (action->kind == ACTION_ITEM) {
        pbitwrite((const uint32_t *)&action->arg_a, 16);
        pbitwrite((const uint32_t *)&action->arg_b, 16);
    }
}

/* Write the actions that the player queued and the server did not see: from the tail of the
 * ring up to the count, and not more than 15. */
static void write_actions(const Player *player)
{
    uint32_t count, index, i;

    if (player->num_actions < player->action_tail)
        count = (uint32_t)(player->num_actions + ACTION_RING - player->action_tail);
    else
        count = (uint32_t)(player->num_actions - player->action_tail);
    if ((int)count > MAX_ACTIONS_PER_PACKET)
        count = MAX_ACTIONS_PER_PACKET;
    pbitwrite(&count, 4);
    pbitwrite((const uint32_t *)&player->action_tail, 8);
    index = (uint32_t)player->action_tail;
    for (i = 0; (int)i < (int)count; i++) {
        write_action(&player->actions[index]);
        index = (index + 1) & ACTION_RING_MASK;
    }
}

/* Write the last frames of voice of the ring of the player: the index of the frame, its size,
 * its mode and its bytes. */
static void write_voice(const VoiceRing *ring)
{
    uint32_t frame;

    for (frame = (uint32_t)ring->head - VOICE_FRAMES_PER_PACKET; (frame &= VOICE_RING_MASK) != (uint32_t)ring->head;
         frame++) {
        pbitwrite(&frame, 8);
        pbitwrite((const uint32_t *)&ring->size[frame], 8);
        pbitwrite((const uint32_t *)&ring->mode[frame], 2);
        cs_pwrite(ring->data[frame], 1, ring->size[frame]);
    }
}

/* Send the input packet of the viewed player. Nothing is sent when no player is in view. */
void __cdecl clientsend(void)
{
    const Player *player;
    uint8_t type = PACKET_PLAYER_INPUT;
    int i;

    if (g_view == -1)
        return;
    player = &g_player[g_view];
    resetsendpacket();
    cs_pwrite(&type, 1, 1);
    cs_pwrite(&g_game.round_number, 4, 1);
    for (i = 0; i < 4; i++)
        cs_pwrite(&player->input_axes[i], 4, 1);
    cs_pwrite(&player->yaw, 4, 1);
    cs_pwrite(&player->input_pitch, 4, 1);
    cs_pwrite(&player->input_extra[0], 4, 1);
    cs_pwrite(&player->input_extra[1], 4, 1);
    cs_pwrite(&player->input_buttons, 4, 1);
    pbitwrite((const uint32_t *)&player->input_mode, 8);
    pbitwrite((const uint32_t *)&player->move_mode, 4);
    pbitwrite(&cl_data_last_event, 16);
    cs_pwrite(&g_view_pos, 4, 3);
    write_actions(player);
    write_voice(&g_speex.voice_ring[g_view]);
    pbitwrite((const uint32_t *)&g_speex.voice_ring[g_view].voice_off, 1);
    pbitwritenego((const uint32_t *)&cl_data_input_wheel, 8);
    sendpacket(cl_data_server_address, cl_data_server_port);
}
