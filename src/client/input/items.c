/* Item controls queue actions in the player action ring.
 * Phone keys queue one action for each digit press.
 * Cash consumes wheel events. Door controls turn a hand target around its hinge.
 */
#include "input/items.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"
#include "sr_math.h"

enum { CASH_BILL_ACTION = 2 };
#define DOOR_REACH 0.875f

/* Queue a digit when its key has a press transition. */
static INPUT_INLINE void phone_digit(int player, int item, int key, int digit)
{
    if (input_pressed(key))
        CL_FN(0x4d5550, void (__cdecl *)(int, int, int))(player, item, digit);
}

/* Queue a digit for each keypad or number-row press, in keypad order. */
void getinputs_phone(int player, int item)
{
    phone_digit(player, item, KEY_PAD_0, 0);
    phone_digit(player, item, KEY_PAD_1, 1);
    phone_digit(player, item, KEY_PAD_2, 2);
    phone_digit(player, item, KEY_PAD_3, 3);
    phone_digit(player, item, KEY_PAD_4, 4);
    phone_digit(player, item, KEY_PAD_5, 5);
    phone_digit(player, item, KEY_PAD_6, 6);
    phone_digit(player, item, KEY_PAD_7, 7);
    phone_digit(player, item, KEY_PAD_8, 8);
    phone_digit(player, item, KEY_PAD_9, 9);
    phone_digit(player, item, KEY_0, 0);
    phone_digit(player, item, KEY_1, 1);
    phone_digit(player, item, KEY_2, 2);
    phone_digit(player, item, KEY_3, 3);
    phone_digit(player, item, KEY_4, 4);
    phone_digit(player, item, KEY_5, 5);
    phone_digit(player, item, KEY_6, 6);
    phone_digit(player, item, KEY_7, 7);
    phone_digit(player, item, KEY_8, 8);
    phone_digit(player, item, KEY_9, 9);
}

/* Queue wheel actions and the R-key bill action. Negative wheel counts are untouched. */
void getinputs_money(int player, int item)
{
    InputMouse *mouse = &cl_data_input_mouse;
    for (; mouse->wheel_up > 0; mouse->wheel_up--)
        CL_FN(0x4d5550, void (__cdecl *)(int, int, int))(player, item, 0);
    for (; mouse->wheel_down > 0; mouse->wheel_down--)
        CL_FN(0x4d5550, void (__cdecl *)(int, int, int))(player, item, 1);
    if (input_pressed(KEY_R))
        CL_FN(0x4d5550, void (__cdecl *)(int, int, int))(
            player, item, sr_items[item].num_bills + CASH_BILL_ACTION);
}

/* Turn the door in the left hand. A missing human or item causes no writes. */
void getinputs_door(int player)
{
    Player *controls = &sr_players[player];
    Human *human;
    Item *door;
    vec3 hinge, target, reach, rotation[3];
    float length;
    int item;
    if (controls->human_id == -1)
        return;
    human = &sr_humans[controls->human_id];
    item = human->inventory[0].count > 0 ? human->inventory[0].items[0] : -1;
    if (item == -1)
        return;
    door = &sr_items[item];
    scaleaddvectors(&hinge, &door->pos, &door->rot[0], -0.5f);
    CL_FN(0x4fc900, void (__cdecl *)(vec3 *))(rotation);
    CL_FN(0x4fca30, void (__cdecl *)(vec3 *, const vec3 *, float))(
        rotation, &sr_data_yaxis, controls->input_axes[0]);
    scaleaddvectors(&target, &hinge, &rotation[0], 1.0f);
    scaleaddvectors(&target, &target, &human->bones[2].rot[0],
                    (float)((double)(float)cl_data_input_mouse.dx / INPUT_WIDTH));
    scaleaddvectors(&target, &target, &human->bones[2].rot[2],
                    (float)((double)(float)cl_data_input_mouse.dy / INPUT_WIDTH));
    subtractvectors(&reach, &target, &human->bones[0].pos);
    reach.y = 0.0f;
    length = vectorlength(&reach);
    if (DOOR_REACH < length) {
        scalevector(&reach, &reach, (float)(1.0 / length));
        scaleaddvectors(&target, &human->bones[0].pos, &reach, DOOR_REACH);
    }
    subtractvectors(&target, &target, &hinge);
    controls->input_axes[0] = (float)x87_atan2((double)target.z, (double)target.x);
}
