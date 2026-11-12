/* Player controls fill four movement axes, viewing angles and action bits.
 * The selected hand supplies phone, cash and door controls.
 * Right drag changes free look or vehicle controls according to the movement mode.
 * Keyboard input can be blocked independently of mouse movement.
 */
#include "input/player_inputs.h"
#include "input/keyboard.h"
#include "input/items.h"
#include "client.h"
#include <items.h>
#include "sr_math.h"
#include "sr_runtime.h"

enum {
    CONTROL_HUMAN = 1, CONTROL_CAR = 2, CONTROL_HELI = 3,
    AXIS_GEAR = 0, AXIS_STEER = 1, AXIS_SHIFT = 2, AXIS_FORWARD = 3,
    INPUT_FIRE = 1, INPUT_LOOK = 2, INPUT_JUMP = 4, INPUT_CROUCH = 8,
    INPUT_SHIFT = 16, INPUT_GRAVE = 32, INPUT_SLOT_1 = 64,
    INPUT_USE = 0x800, INPUT_RELOAD = 0x1000, INPUT_DROP = 0x2000,
    INPUT_PAD_0 = 0x4000, INPUT_LEAVE = 0x40000,
    INVENTORY_KEYS = 5, HELI_PAD_KEYS = 4, INPUT_AXIS_COUNT = 4,
    MOVE_SLOW = 1, MOVE_RAISED = 2
};
#define LOOK_SCALE 512.0
#define HELI_LOOK_SCALE 650.0
#define CAR_MOUSE_SCALE 150.0
#define HUMAN_PI 3.14159265359
#define HUMAN_FULL_TURN 6.28318530718
#define INPUT_PI 3.1415927410125732f
#define INPUT_HALF_PI 1.5707963705062866f
#define INPUT_FREE_LOOK 2.7488934993743896f
#define CAR_STEER_RATE 0.03750000149011612
#define CAR_STEER_BOUND (0.75 - CAR_STEER_RATE)
#define CAR_STEER_LIMIT 0.75f
#define DRIVE_RATE 0.03125
#define CAR_FORWARD_RETURN 0.125
#define CAR_GEAR_FIRST_SPLIT 0.5f
#define CAR_GEAR_SECOND_SPLIT 1.5f
#define CAR_GEAR_MIN (-0.125f)
#define CAR_GEAR_MAX 2.125f
#define CAR_SHIFT_DEADZONE 0.375f
#define CAR_ZOOM_LIMIT 0.625f
#define HELI_ZOOM_LIMIT 0.875f
#define HUMAN_ZOOM_SLOW 0.0625f
#define HUMAN_ZOOM_RAISED 0.5f

/* Return the first item of the selected hand, or -1 for an empty hand. */
static int selected_item(const Human *human)
{
    const InventorySlot *hand = &human->inventory[cl_data_audio_keys[KEY_SHIFT] != 0];
    return hand->count > 0 ? hand->items[0] : -1;
}

/* Consume wheel events into the target zoom and smooth its bounded value. */
static INPUT_INLINE void vehicle_zoom(Player *player, float limit)
{
    InputMouse *mouse = &cl_data_input_mouse;
    for (; mouse->wheel_up > 0; mouse->wheel_up--)
        player->zoom_target = (float)((double)player->zoom_target + 0.125);
    for (; mouse->wheel_down > 0; mouse->wheel_down--)
        player->zoom_target = (float)((double)player->zoom_target - 0.125);
    if (player->zoom_target < 0.0f) player->zoom_target = 0.0f;
    if (limit < player->zoom_target) player->zoom_target = limit;
    player->zoom = (float)(((double)player->zoom_target - player->zoom) * 0.125 * 0.5 + player->zoom);
}

/* Add view movement with the configured vertical sign. */
static INPUT_INLINE void view_move(float *yaw, float *pitch, float sensitivity)
{
    const InputMouse *mouse = &cl_data_input_mouse;
    *yaw = (float)((double)(float)mouse->dx * sensitivity / LOOK_SCALE + *yaw);
    if (!sr_data_option.invertmouse)
        *pitch = (float)((double)(float)mouse->dy * sensitivity / LOOK_SCALE + *pitch);
    else
        *pitch = (float)(*pitch - (double)(float)mouse->dy * sensitivity / LOOK_SCALE);
}

/* Limit a view while permitting unordered values to pass through. */
static INPUT_INLINE void view_limit(float *yaw, float *pitch, float yaw_limit)
{
    if (*yaw < -yaw_limit) *yaw = -yaw_limit;
    if (yaw_limit < *yaw) *yaw = yaw_limit;
    if (*pitch < -INPUT_HALF_PI) *pitch = -INPUT_HALF_PI;
    if (INPUT_HALF_PI < *pitch) *pitch = INPUT_HALF_PI;
}

/* Queue item digits or set inventory slot bits. */
static INPUT_INLINE void item_buttons(int player_index, Player *player, int item)
{
    int key;
    if (item != -1 && sr_items[item].type == ITEM_CELL_PHONE && sr_items[item].call_state == 1) {
        getinputs_phone(player_index, item);
    } else {
        for (key = 0; key < INVENTORY_KEYS; key++)
            if (cl_data_audio_keys[KEY_1 + key])
                player->input_buttons |= INPUT_SLOT_1 << key;
    }
    if (cl_data_audio_keys[KEY_E]) player->input_buttons |= INPUT_USE;
    if (cl_data_audio_keys[KEY_R]) player->input_buttons |= INPUT_RELOAD;
    if (cl_data_audio_keys[KEY_F]) player->input_buttons |= INPUT_DROP;
}

/* Fill on-foot controls. A missing human clears only movement and action fields. */
void getinputs_human(int player_index)
{
    Player *player = &sr_players[player_index];
    Human *human;
    int item;
    float sensitivity;
    player->input_axes[AXIS_STEER] = 0.0f;
    player->input_axes[AXIS_SHIFT] = 0.0f;
    player->input_axes[AXIS_FORWARD] = 0.0f;
    player->input_buttons = 0;
    if (player->human_id == -1)
        return;
    human = &sr_humans[player->human_id];
    if (player->unused_cc != CONTROL_HUMAN) {
        volatile double axis, pitch;
        player->input_axes[AXIS_GEAR] = 0.0f;
        /* Float conversion quiets signaling NaNs before assignment. */
        axis = human->axis;
        player->yaw = (float)axis;
        pitch = human->pitch;
        player->input_pitch = (float)pitch;
        player->crouch_toggle = 0;
        CL_FN(0x46ab70, void (__cdecl *)(const char *, float))(
            cl_data_input_label_human_axis, player->yaw);
    }
    if (cl_data_input_cursor)
        return;
    item = selected_item(human);
    /* A changed empty hand reads the type word before the item array. */
    if (player->held_item_previous != item && sr_items[item].type == ITEM_DOOR)
        player->input_axes[AXIS_GEAR] = (float)x87_atan2(sr_items[item].rot[0].z, sr_items[item].rot[0].x);
    player->held_item_previous = item;
    if (item != -1 && sr_items[item].type == ITEM_CASH_2) {
        getinputs_money(player_index, item);
    } else {
        for (; cl_data_input_mouse.wheel_up > 0; cl_data_input_mouse.wheel_up--)
            player->move_mode++;
        for (; cl_data_input_mouse.wheel_down > 0; cl_data_input_mouse.wheel_down--)
            player->move_mode--;
    }
    if (player->move_mode < 0) player->move_mode = 0;
    if (player->move_mode > MOVE_RAISED) player->move_mode = MOVE_RAISED;
    if (player->move_mode == 0) player->zoom_target = 0.0f;
    if (player->move_mode == MOVE_SLOW) player->zoom_target = HUMAN_ZOOM_SLOW;
    if (player->move_mode == MOVE_RAISED) player->zoom_target = HUMAN_ZOOM_RAISED;
    player->zoom = (float)(((double)player->zoom_target - player->zoom) * 0.125 * 0.5 + player->zoom);
    sensitivity = (float)(1.0 - player->zoom);
    if (!cl_data_input_mouse.right) {
        if (item != -1 && sr_items[item].type == ITEM_DOOR)
            getinputs_door(player_index);
        else
            view_move(&player->yaw, &player->input_pitch, sensitivity);
        player->yaw = (float)((double)player->input_extra[0] * 0.125 + player->yaw);
        player->input_pitch = (float)((double)player->input_extra[1] * 0.125 + player->input_pitch);
        player->input_extra[0] = (float)((double)player->input_extra[0] * 0.875);
        player->input_extra[1] = (float)((double)player->input_extra[1] * 0.875);
    } else {
        view_move(&player->input_extra[0], &player->input_extra[1], sensitivity);
        player->input_buttons |= INPUT_LOOK;
    }
    if (player->yaw < -HUMAN_PI)
        player->yaw = (float)(player->yaw + HUMAN_FULL_TURN);
    if (HUMAN_PI < player->yaw)
        player->yaw = (float)(player->yaw - HUMAN_FULL_TURN);
    if (player->input_pitch < -INPUT_HALF_PI) player->input_pitch = -INPUT_HALF_PI;
    if (INPUT_HALF_PI < player->input_pitch) player->input_pitch = INPUT_HALF_PI;
    view_limit(&player->input_extra[0], &player->input_extra[1], INPUT_FREE_LOOK);
    if (player->crouch_toggle) player->input_buttons |= INPUT_CROUCH;
    if (cl_data_audio_input_blocked)
        return;
    if (cl_data_audio_keys[KEY_W]) player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] + 1.0);
    if (cl_data_audio_keys[KEY_S]) player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] - 1.0);
    if (cl_data_audio_keys[KEY_A]) player->input_axes[AXIS_STEER] = (float)((double)player->input_axes[AXIS_STEER] - 1.0);
    if (cl_data_audio_keys[KEY_D]) player->input_axes[AXIS_STEER] = (float)((double)player->input_axes[AXIS_STEER] + 1.0);
    if (cl_data_input_mouse.left && !cl_data_input_fire_blocked) player->input_buttons |= INPUT_FIRE;
    if (cl_data_audio_keys[KEY_SPACE]) player->input_buttons |= INPUT_JUMP;
    if (cl_data_audio_keys[KEY_SHIFT]) player->input_buttons |= INPUT_SHIFT;
    if (cl_data_audio_keys[KEY_GRAVE]) player->input_buttons |= INPUT_GRAVE;
    if (cl_data_audio_keys[KEY_DELETE]) player->input_buttons |= INPUT_LEAVE;
    if (input_pressed(KEY_CONTROL)) player->crouch_toggle ^= 1;
    item_buttons(player_index, player, item);
}

/* Fill car controls. A menu, missing human or missing vehicle clears only action bits. */
void getinputs_car(int player_index)
{
    Player *player = &sr_players[player_index];
    int item;
    float sensitivity;
    player->input_buttons = 0;
    if (cl_data_input_cursor || player->human_id == -1 || sr_humans[player->human_id].vehicle_id == -1)
        return;
    item = selected_item(&sr_humans[player->human_id]);
    player->move_mode = MOVE_SLOW;
    if (player->unused_cc != CONTROL_CAR) {
        player->yaw = 0.0f;
        player->input_pitch = 0.0f;
        player->input_axes[AXIS_GEAR] = 2.0f;
        player->input_axes[AXIS_SHIFT] = -1.0f;
    }
    if (item != -1 && sr_items[item].type == ITEM_CASH_2)
        getinputs_money(player_index, item);
    vehicle_zoom(player, CAR_ZOOM_LIMIT);
    sensitivity = (float)(1.0 - player->zoom);
    if (!cl_data_input_mouse.right) {
        view_move(&player->yaw, &player->input_pitch, sensitivity);
        if (player->input_axes[AXIS_GEAR] < CAR_GEAR_FIRST_SPLIT)
            player->input_axes[AXIS_GEAR] = 0.0f;
        else
            player->input_axes[AXIS_GEAR] = player->input_axes[AXIS_GEAR] < CAR_GEAR_SECOND_SPLIT ? 1.0f : 2.0f;
        if (player->input_axes[AXIS_SHIFT] < -CAR_SHIFT_DEADZONE)
            player->input_axes[AXIS_SHIFT] = -1.0f;
        else
            player->input_axes[AXIS_SHIFT] = CAR_SHIFT_DEADZONE < player->input_axes[AXIS_SHIFT] ? 1.0f : 0.0f;
    } else {
        player->input_axes[AXIS_GEAR] = (float)((double)(float)cl_data_input_mouse.dx / CAR_MOUSE_SCALE + player->input_axes[AXIS_GEAR]);
        player->input_axes[AXIS_SHIFT] = (float)((double)(float)cl_data_input_mouse.dy / CAR_MOUSE_SCALE + player->input_axes[AXIS_SHIFT]);
        if (player->input_axes[AXIS_GEAR] < CAR_GEAR_MIN) {
            player->input_axes[AXIS_GEAR] = CAR_GEAR_MIN;
        } else {
            if (CAR_GEAR_MAX < player->input_axes[AXIS_GEAR]) player->input_axes[AXIS_GEAR] = CAR_GEAR_MAX;
        }
        if (player->input_axes[AXIS_SHIFT] < -1.0f) {
            player->input_axes[AXIS_SHIFT] = -1.0f;
        } else {
            if (1.0f < player->input_axes[AXIS_SHIFT]) player->input_axes[AXIS_SHIFT] = 1.0f;
        }
    }
    view_limit(&player->yaw, &player->input_pitch, INPUT_PI);
    if (cl_data_input_mouse.right) player->input_buttons |= INPUT_LOOK;
    if (!cl_data_audio_input_blocked) {
        if (cl_data_audio_keys[KEY_W]) {
            player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] + DRIVE_RATE);
        } else if (cl_data_audio_keys[KEY_S]) {
            player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] - DRIVE_RATE);
        } else if (CAR_FORWARD_RETURN <= player->input_axes[AXIS_FORWARD]) {
            player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] - CAR_FORWARD_RETURN);
        } else if (player->input_axes[AXIS_FORWARD] <= -CAR_FORWARD_RETURN) {
            player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] + CAR_FORWARD_RETURN);
        } else {
            player->input_axes[AXIS_FORWARD] = 0.0f;
        }
        if (cl_data_audio_keys[KEY_A]) {
            if (-CAR_STEER_BOUND < player->input_axes[AXIS_STEER])
                player->input_axes[AXIS_STEER] = (float)(player->input_axes[AXIS_STEER] - CAR_STEER_RATE);
            else
                player->input_axes[AXIS_STEER] = -CAR_STEER_LIMIT;
        } else if (player->input_axes[AXIS_STEER] < -CAR_STEER_RATE) {
            player->input_axes[AXIS_STEER] = (float)(player->input_axes[AXIS_STEER] + CAR_STEER_RATE);
        } else {
            if (player->input_axes[AXIS_STEER] < 0.0f) player->input_axes[AXIS_STEER] = 0.0f;
        }
        if (cl_data_audio_keys[KEY_D]) {
            if (player->input_axes[AXIS_STEER] < CAR_STEER_BOUND)
                player->input_axes[AXIS_STEER] = (float)(player->input_axes[AXIS_STEER] + CAR_STEER_RATE);
            else
                player->input_axes[AXIS_STEER] = CAR_STEER_LIMIT;
        } else if (CAR_STEER_RATE < player->input_axes[AXIS_STEER]) {
            player->input_axes[AXIS_STEER] = (float)(player->input_axes[AXIS_STEER] - CAR_STEER_RATE);
        } else {
            if (0.0f < player->input_axes[AXIS_STEER]) player->input_axes[AXIS_STEER] = 0.0f;
        }
        if (cl_data_input_mouse.left && !cl_data_input_fire_blocked) player->input_buttons |= INPUT_FIRE;
        if (cl_data_audio_keys[KEY_SPACE]) player->input_buttons |= INPUT_JUMP;
        if (cl_data_audio_keys[KEY_CONTROL]) player->input_buttons |= INPUT_CROUCH;
        if (cl_data_audio_keys[KEY_SHIFT]) player->input_buttons |= INPUT_SHIFT;
        if (cl_data_audio_keys[KEY_GRAVE]) player->input_buttons |= INPUT_GRAVE;
        item_buttons(player_index, player, item);
        if (cl_data_audio_keys[KEY_DELETE]) player->input_buttons |= INPUT_LEAVE;
    }
    if (player->input_axes[AXIS_FORWARD] < -1.0f) player->input_axes[AXIS_FORWARD] = -1.0f;
    if (1.0f < player->input_axes[AXIS_FORWARD]) player->input_axes[AXIS_FORWARD] = 1.0f;
}

/* Decay one mouse control toward zero. Unordered input becomes zero. */
static float heli_decay(float value)
{
    if (value < -1.0f) return (float)((double)value + 1.0);
    if (1.0f < value) return (float)((double)value - 1.0);
    return 0.0f;
}

/* Fill helicopter controls. A menu permits mode initialization but blocks movement. */
void getinputs_heli(int player_index)
{
    Player *player = &sr_players[player_index];
    InputMouse *mouse = &cl_data_input_mouse;
    float sensitivity;
    int key;
    if (player->unused_cc != CONTROL_HELI) {
        player->input_axes[AXIS_FORWARD] = 0.0f;
        player->yaw = 0.0f;
        player->input_pitch = 0.0f;
    }
    player->move_mode = MOVE_SLOW;
    player->input_axes[AXIS_GEAR] = 0.0f;
    player->input_axes[AXIS_STEER] = 0.0f;
    player->input_axes[AXIS_SHIFT] = 0.0f;
    player->input_buttons = 0;
    if (cl_data_input_cursor)
        return;
    sensitivity = (float)(1.0 - player->zoom);
    if (!mouse->right || mouse->left) {
        cl_data_input_heli_mouse[0] = heli_decay(cl_data_input_heli_mouse[0]);
        cl_data_input_heli_mouse[1] = heli_decay(cl_data_input_heli_mouse[1]);
        cl_data_input_heli_mouse[0] = (float)((double)(float)mouse->dx * 0.125 * 0.5 + cl_data_input_heli_mouse[0]);
        cl_data_input_heli_mouse[1] = (float)((double)(float)mouse->dy * 0.125 * 0.5 + cl_data_input_heli_mouse[1]);
        player->input_axes[AXIS_SHIFT] = (float)((double)player->input_axes[AXIS_SHIFT] + cl_data_input_heli_mouse[0]);
        player->input_axes[AXIS_GEAR] = (float)((double)player->input_axes[AXIS_GEAR] - cl_data_input_heli_mouse[1]);
        if (!mouse->left) {
            player->yaw = (float)((double)player->yaw * 0.875);
            player->input_pitch = (float)((double)player->input_pitch * 0.875);
        }
    } else {
        player->yaw = (float)((double)(float)mouse->dx * sensitivity / HELI_LOOK_SCALE + player->yaw);
        player->input_pitch = (float)((double)(float)mouse->dy * sensitivity / HELI_LOOK_SCALE + player->input_pitch);
        view_limit(&player->yaw, &player->input_pitch, INPUT_FREE_LOOK);
    }
    vehicle_zoom(player, HELI_ZOOM_LIMIT);
    if (!cl_data_audio_input_blocked) {
        if (cl_data_audio_keys[KEY_W]) player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] + DRIVE_RATE);
        if (cl_data_audio_keys[KEY_S]) player->input_axes[AXIS_FORWARD] = (float)((double)player->input_axes[AXIS_FORWARD] - DRIVE_RATE);
        if (cl_data_audio_keys[KEY_A]) player->input_axes[AXIS_STEER] = (float)((double)player->input_axes[AXIS_STEER] - 1.0);
        if (cl_data_audio_keys[KEY_D]) player->input_axes[AXIS_STEER] = (float)((double)player->input_axes[AXIS_STEER] + 1.0);
        if (mouse->left && !cl_data_input_fire_blocked) player->input_buttons |= INPUT_FIRE;
        if (cl_data_audio_keys[KEY_SPACE]) player->input_buttons |= INPUT_JUMP;
        if (cl_data_audio_keys[KEY_CONTROL]) player->input_buttons |= INPUT_CROUCH;
        if (cl_data_audio_keys[KEY_SHIFT]) player->input_buttons |= INPUT_SHIFT;
        for (key = 0; key < INVENTORY_KEYS; key++)
            if (cl_data_audio_keys[KEY_1 + key]) player->input_buttons |= INPUT_SLOT_1 << key;
        if (cl_data_audio_keys[KEY_E]) player->input_buttons |= INPUT_USE;
        if (cl_data_audio_keys[KEY_R]) player->input_buttons |= INPUT_RELOAD;
        if (cl_data_audio_keys[KEY_PAD_0]) player->input_buttons |= INPUT_PAD_0;
        for (key = 1; key < HELI_PAD_KEYS; key++)
            if (cl_data_audio_keys[KEY_PAD_1 + key - 1]) player->input_buttons |= INPUT_PAD_0 << key;
    }
    for (key = 0; key < INPUT_AXIS_COUNT; key++) {
        if (player->input_axes[key] < -1.0f) player->input_axes[key] = -1.0f;
        if (1.0f < player->input_axes[key]) player->input_axes[key] = 1.0f;
    }
}
