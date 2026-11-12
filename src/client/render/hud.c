/* The head up display of the viewed player: the map, the phone help, the stance and the voice
 * labels, the items in the hands and in the other slots, the meters of a vehicle and the aim marks. */
#include "render/hud.h"

#include "client.h"
#include "render/draw2d.h"
#include "render/indicators.h"
#include "render/map.h"
#include "render/text.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define GL_LINES 0x0001

#define g_game_state cl_data_game_state
#define g_keyboard cl_data_keyboard
#define g_voice_mode cl_data_audio_voice_mode
#define g_map_shown cl_data_map_shown
#define g_spectated_human cl_data_spectated_human
#define g_free_for_all cl_data_free_for_all
#define g_view_zoom cl_data_view_zoom

/* Game functions of other files. */
#define vectorlength CL_FN(0x50b890, double(__cdecl *)(const float *vector))

enum {
    NO_PLAYER = -1,
    NO_HUMAN = -1,
    NO_VEHICLE = -1,
    STATE_ROUND_END = 3,
    ITEM_PHONE = 0x17,
    PHONE_IDLE = 0,
    PHONE_DIAL = 1,
    PHONE_RING = 2,
    PHONE_TALK = 3,
    INPUT_MODE_CAR = 2,
    INPUT_MODE_HELI = 3,
    SEAT_DRIVER = 0,
    MOVE_RUN = 0,
    MOVE_WALK = 1,
    MOVE_AIM = 2,
    VOICE_WHISPER = 0,
    VOICE_YELL = 2,
    BUTTON_NO_AIM_MARK = 0x10,
    KEY_LEFT_SHIFT = 0xe1,
    HAND_SLOTS = 2,
    FIRST_PACK_SLOT = 2,
    END_PACK_SLOT = 7,
    LABEL_FLAGS = 8,
    HELP_FLAGS = 1,
    HAND_FLAGS = 0x29,
    PACK_NUMBER_FLAGS = 0x20,
    PACK_NAME_FLAGS = 0x28
};
#define TEXTURE_2D 0x0de1

/* The line of help for a phone: the text, centered at x 512 and y row. */
static void phone_help(const char *text, float row)
{
    drawtext(text, 512.0f, row, 16.0f, HELP_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* The help for the phone in the right hand. */
static void draw_phone(const Item *phone)
{
    float row = 320.0f;

    if (phone->call_state == PHONE_IDLE)
        phone_help("Left-click to dial", 400.0f);
    if (phone->call_state == PHONE_DIAL) {
        phone_help("Use Numpad or number keys", 400.0f);
        phone_help("1111 - Goldmen", 416.0f);
        phone_help("2222 - Monsota", 432.0f);
        phone_help("3333 - OXS", 448.0f);
        drawtext("4444 - /s", 512.0f, 464.0f, 16.0f, HELP_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f, sr_data_teamname[3]);
        drawtext("5555 - /s", 512.0f, 480.0f, 16.0f, HELP_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f, sr_data_teamname[4]);
        row = 496.0f;
    }
    if (phone->call_state == PHONE_RING) {
        if (phone->info < 1)
            phone_help("Left-click to hang up", 400.0f);
        else
            phone_help("Left-click to answer", 400.0f);
    }
    if (phone->call_state == PHONE_TALK)
        drawtext("In Use", 640.0f, row, 24.0f, HELP_FLAGS, 0.0f, 1.0f, 0.0f, 1.0f);
    if ((phone->call_state == PHONE_DIAL || phone->call_state == PHONE_RING) && 0 < phone->info)
        drawtext("Number:/i", 640.0f, (float)(row + 24.0), 24.0f, HELP_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f, phone->info);
}

/* The stance, the health and the voice labels at the bottom left. */
static void draw_labels(int player, const Human *human)
{
    const Player *entry = &sr_data_player[player];

    if (entry->crouching != 0)
        drawtext("Crouch", 4.0f, 612.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    if (human->bleeding == 0)
        drawtext("/i", 4.0f, 636.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f, entry->hud_health);
    else
        drawtext("/i", 4.0f, 636.0f, 24.0f, LABEL_FLAGS, 0.75f, 0.0f, 0.0f, 1.0f, entry->hud_health);
    if (entry->move_mode == MOVE_RUN)
        drawtext("Run", 64.0f, 636.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    else if (entry->move_mode == MOVE_WALK)
        drawtext("Walk", 64.0f, 636.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    else if (entry->move_mode == MOVE_AIM)
        drawtext("Aim", 64.0f, 636.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    if (g_voice_mode == VOICE_WHISPER)
        drawtext("Whisper", 4.0f, 612.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    if (g_voice_mode == VOICE_YELL)
        drawtext("Yell", 4.0f, 612.0f, 24.0f, LABEL_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    if (entry->menu != 0)
        drawtext("Press Tab for Menu", 4.0f, 24.0f, 24.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* The items of the two hands. The hand that the keyboard selects is white and larger. */
static void draw_hands(const Player *player)
{
    int hand;
    int index;

    for (hand = 0; hand < HAND_SLOTS; ++hand) {
        for (index = 0; index < player->hud_inventory[hand].count; ++index) {
            float size = 16.0f;
            float red = 0.5f;
            float green = 0.5f;
            float blue = 0.5f;
            unsigned code = (unsigned)player->hud_inventory[hand].items[index];
            const ItemType *type = &sr_itemtypes[code & 0xff];
            int magazine = (int)code >> 8;
            float x;

            if (hand == 0 && g_keyboard[KEY_LEFT_SHIFT] == 0) {
                red = green = blue = 1.0f;
                size = 18.0f;
            } else if (hand == 1 && g_keyboard[KEY_LEFT_SHIFT] != 0) {
                red = green = blue = 1.0f;
                size = 18.0f;
            }
            x = (float)(576.0 - (float)hand * 128.0);
            drawtext(type->name, (float)(x + 8.0), 588.0f, size, HAND_FLAGS, red, green, blue, 1.0f);
            if (type->ammo < 1) {
                if (0 < magazine) {
                    if (magazine == 1)
                        drawtext("+magazine", (float)(x + 8.0), (float)(size * 0.5 + 588.0), (float)(size * 0.75),
                                 HAND_FLAGS, red, green, blue, 1.0f);
                    else
                        drawtext("+empty magazine", (float)(x + 8.0), (float)(size * 0.5 + 588.0),
                                 (float)(size * 0.75), HAND_FLAGS, red, green, blue, 1.0f);
                }
            } else if (magazine < 1) {
                drawtext("/i", (float)(x + 8.0), (float)(size * 0.5 + 588.0), (float)(size * 0.75), HAND_FLAGS, red,
                         0.0f, 0.0f, 1.0f, magazine);
            } else {
                drawtext("/i", (float)(x + 8.0), (float)(size * 0.5 + 588.0), (float)(size * 0.75), HAND_FLAGS, red,
                         green, blue, 1.0f, magazine);
            }
        }
    }
}

/* The names of the items in the five slots of the pack, one column for each slot. */
static void draw_pack(const Player *player)
{
    float x = 212.0f;
    int slot;
    int index;

    for (slot = FIRST_PACK_SLOT; slot < END_PACK_SLOT; ++slot) {
        float row = 612.0f;

        drawtext("/i", x, 596.0f, 12.0f, PACK_NUMBER_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f, slot - 1);
        for (index = 0; index < player->hud_inventory[slot].count; ++index) {
            drawtext(sr_itemtypes[player->hud_inventory[slot].items[index] & 0xff].name, (float)(x + 8.0), row,
                     16.0f, PACK_NAME_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
            row = (float)(row + 16.0);
        }
        x = (float)(x + 128.0);
    }
}

/* A bar of the gear of a helicopter or the stick of a car: a dark frame and a white mark. */
static void draw_frame(float x, float y, float width, float height)
{
    drawbitmap(-1, x, y, width, height, 5, 0.0f, 0.0f, 0.0f, 0.5f);
}

static void draw_mark(float x, float y)
{
    drawbitmap(-1, x, y, 16.0f, 16.0f, 5, 1.0f, 1.0f, 1.0f, 0.5f);
}

/* The meters of the vehicle that the human drives: the stick for the input mode 3, and the
 * speed, the engine and the stick for the input mode 2. */
static void draw_vehicle(const Player *player, const Human *human)
{
    int vehicle = human->vehicle_id;

    if (vehicle != NO_VEHICLE && human->vehicle_seat == SEAT_DRIVER && player->input_mode == INPUT_MODE_HELI) {
        draw_frame(818.0f, 512.0f, 16.0f, 80.0f);
        draw_mark(818.0f, (float)(512.0 - (player->input_axes[3] * 16.0 + player->input_axes[3] * 16.0)));
    }
    if (vehicle != NO_VEHICLE && human->vehicle_seat == SEAT_DRIVER && player->input_mode == INPUT_MODE_CAR) {
        double speed = vectorlength(&sr_data_object[vehicle].vel.x);
        float turn;

        turn = (float)((speed * 60.0 * 60.0 * 60.0 * 0.6213712096214294) / 1000.0);
        turn = (float)(turn / 180.0);
        turn = (float)(turn * 6.28318530718);
        drawcircleindicator(548.0f, 544.0f, 36.0f, (float)(turn - 2.3561944901925003));
        turn = (float)((float)sr_data_object[vehicle].rpm_net / 5000.0);
        turn = (float)(turn * 3.14159265359);
        drawcircleindicator(480.0f, 544.0f, 32.0f, (float)(turn - 2.3561944901925003));
        draw_frame(818.0f, 512.0f, 16.0f, 80.0f);
        draw_frame(850.0f, 512.0f, 16.0f, 80.0f);
        draw_frame(850.0f, 512.0f, 80.0f, 16.0f);
        draw_frame(882.0f, 528.0f, 16.0f, 48.0f);
        draw_mark((float)(player->input_axes[0] * 16.0 + player->input_axes[0] * 16.0 + 818.0),
                  (float)(player->input_axes[2] * 16.0 + player->input_axes[2] * 16.0 + 512.0));
    }
}

/* The four short marks around the middle of the screen, while a gun is in the right hand and
 * the aim mark is not off. */
static void draw_aim_marks(const Player *player)
{
    float gap;

    if ((player->input_buttons & BUTTON_NO_AIM_MARK) != 0 || player->hud_inventory[0].count <= 0 ||
        sr_itemtypes[player->hud_inventory[0].items[0] & 0xff].is_gun == 0)
        return;
    glDisable(TEXTURE_2D);
    glBegin(GL_LINES);
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    gap = (float)(8.0 / g_view_zoom);
    glVertex2f(512.0f, (float)(320.0 - (gap + gap)));
    glVertex2f(512.0f, (float)(320.0 - gap));
    glVertex2f(512.0f, (float)(gap + 320.0));
    glVertex2f(512.0f, (float)(gap + gap + 320.0));
    glVertex2f((float)(512.0 - (gap + gap)), 320.0f);
    glVertex2f((float)(512.0 - gap), 320.0f);
    glVertex2f((float)(gap + 512.0), 320.0f);
    glVertex2f((float)(gap + gap + 512.0), 320.0f);
    glEnd();
    glEnable(TEXTURE_2D);
}

/* Draw the display of a player. A player with no human sees only the map. */
void __cdecl drawhud(int player)
{
    int index;
    const Human *human;
    const Item *phone;

    if (player == NO_PLAYER)
        return;
    index = sr_data_player[player].human_id;
    if (g_game_state == STATE_ROUND_END)
        return;
    if (index == NO_HUMAN) {
        if (g_map_shown != 0 && g_free_for_all == 0)
            drawmap(g_spectated_human);
        return;
    }
    if (g_map_shown != 0 && g_free_for_all == 0)
        drawmap(index);
    human = &sr_data_human[index];
    phone = &sr_data_item[human->inventory[0].items[0]];
    if (0 < human->inventory[0].count && phone->type == ITEM_PHONE)
        draw_phone(phone);
    draw_labels(player, human);
    draw_hands(&sr_data_player[player]);
    draw_pack(&sr_data_player[player]);
    draw_vehicle(&sr_data_player[player], human);
    draw_aim_marks(&sr_data_player[player]);
}
