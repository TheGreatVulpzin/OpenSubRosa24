/* humansimulation (RVA 0x51060): the people: bones, muscles, items in hand, rewritten by hand.
 *
 * Phase 6 (docs/plano.md). The first version is the lifted C, unchanged. Each step makes
 * it readable and faster and keeps the harness at 0 differences: the float math keeps its
 * order and its (float) rounding points (x87, 53-bit precision, as the original).
 *
 * The function keeps the symbol lf_51060: tools/lift/lift.py sees the definition here
 * and lifts it no more, but keeps it in the table, the live hooks and the standalone server.
 */
#include "lift_support.h"
#include "lift_os.h"

#define g_human (*(Human (*)[512])SR_VA(0x19cbf80))
#define g_body (*(Body (*)[32])SR_VA(0xf670280))
#define DAT_00460d18 (*(uint *)SR_VA(0x460d18))
#define g_object (*(Object (*)[1024])SR_VA(0x5992600))
#define g_game (*(Game *)SR_VA(0x94dc540))
#define g_item (*(Item (*)[1024])SR_VA(0x95022c0))
#define g_soundlist (*(SoundList *)SR_VA(0x956d700))
#define g_particle (*(Particle (*)[4096])SR_VA(0xb7e7200))
#define DAT_0f837b64 (*(int *)SR_VA(0xf837b64))
#define g_city (*(City *)SR_VA(0xe699220))
#define g_player (*(Player (*)[256])SR_VA(0xf1edd00))
#define g_account (*(Account (*)[65536])SR_VA(0xf837b68))
#define g_intersect (*(Intersect *)SR_VA(0x107484a0))
#define g_timerlist (*(TimerList *)SR_VA(0x12e81c60))
#define g_xaxis (*(float (*)[3])SR_VA(0x460cd4))
#define g_yaxis (*(float (*)[3])SR_VA(0x460ce0))
#define g_zaxis (*(float (*)[3])SR_VA(0x460cec))
#define g_numofbonds (*(int *)SR_VA(0x17579a28))
#define g_bond (*(Bond (*)[16384])SR_VA(0x17579a40))
#define g_timer (*(Timer (*)[256])SR_VA(0x18043a0))
#define playersellstock ((void (__cdecl *)(float, int))SR_FN(0x405be0))
#define createevent_updateitem ((int (__cdecl *)(uint))SR_FN(0x406380))
#define createevent_updateplayer ((void (__cdecl *)(int))SR_FN(0x4064a0))
#define item_removehumanlink ((void (__cdecl *)(uint, uint *, int))SR_FN(0x407e50))
#define item_removeitemlink ((void (__cdecl *)(uint, uint))SR_FN(0x407f80))
#define pointinsidebuilding ((undefined4 (__cdecl *)(int, float *, float))SR_FN(0x408a60))
#define stoptimer ((void (__cdecl *)(int))SR_FN(0x40b3f0))
#define scaleaddvectors ((void (__cdecl *)(float *, float *, float *, float))SR_FN(0x40ba60))
#define rotateorientation ((void (__cdecl *)(float *, float *, float))SR_FN(0x40bdb0))
#define playerdeathtax ((void (__cdecl *)(uint))SR_FN(0x416960))
#define playerteamkilltax ((void (__cdecl *)(int, int))SR_FN(0x416a20))
#define createevent_bullethit ((void (__cdecl *)(int, int, undefined4 *, undefined4 *))SR_FN(0x416c40))
#define createevent_sound ((void (__cdecl *)(int, undefined4 *, float, float))SR_FN(0x416db0))
#define deletehuman ((void (__cdecl *)(uint *))SR_FN(0x418720))
#define human_calculatelegangles ((void (__cdecl *)(int))SR_FN(0x419060))
#define human_collisionhuman ((void (__cdecl *)(float))SR_FN(0x419380))
#define getobjectrotation2 ((void (__cdecl *)(float *, int, float *))SR_FN(0x4223d0))
#define lineintersectobject ((undefined4 (__cdecl *)(int, float *, float *, int))SR_FN(0x4257a0))
#define lineintersectlevel ((undefined4 (__cdecl *)(float *, float *, int))SR_FN(0x431630))
#define sphereintersectobject ((undefined4 (__cdecl *)(int, float *, float))SR_FN(0x432bb0))
#define human_calculatearmangles ((void (__cdecl *)(int))SR_FN(0x44a440))
#define human_itemsimulation ((void (__cdecl *)(int))SR_FN(0x44bd60))
#define sphereintersectlevel ((undefined4 (__cdecl *)(float *, float))SR_FN(0x44d750))
#define __security_check_cookie(x) ((void)0)
#define _ftol2(x) ((ulonglong)(longlong)(x))
#define _CIcos x87_cos
#define _CIsin x87_sin
#define _CIsqrt x87_sqrt
#define _CIatan2 x87_atan2
#define local_cc (*(int * *)(lf_frame + 0x14))
#define local_c8 (*(float *)(lf_frame + 0x18))
#define local_c4 (*(Bone * *)(lf_frame + 0x1c))
#define local_c0 (*(float *)(lf_frame + 0x20))
#define local_bc (*(float *)(lf_frame + 0x24))
#define local_b8 (*(float *)(lf_frame + 0x28))
#define local_b4 (*(float *)(lf_frame + 0x2c))
#define local_b0 (*(float *)(lf_frame + 0x30))
#define local_ac (*(float *)(lf_frame + 0x34))
#define local_a8 (*(float *)(lf_frame + 0x38))
#define local_a4 (*(uint * *)(lf_frame + 0x3c))
#define local_a0 (*(float *)(lf_frame + 0x40))
#define local_9c (*(int *)(lf_frame + 0x44))
#define local_98 (*(double *)(lf_frame + 0x48))
#define local_90 (*(float *)(lf_frame + 0x50))
#define local_8c (*(float *)(lf_frame + 0x54))
#define local_88 (*(float *)(lf_frame + 0x58))
#define local_84 (*(float *)(lf_frame + 0x5c))
#define local_80 (*(float *)(lf_frame + 0x60))
#define local_7c (*(float *)(lf_frame + 0x64))
#define local_78 (*(float *)(lf_frame + 0x68))
#define local_74 (*(float *)(lf_frame + 0x6c))
#define local_70 (*(float *)(lf_frame + 0x70))
#define local_6c (*(float *)(lf_frame + 0x74))
#define local_68 (*(float *)(lf_frame + 0x78))
#define local_64 (*(float *)(lf_frame + 0x7c))
#define local_60 (*(float *)(lf_frame + 0x80))
#define local_5c (*(float *)(lf_frame + 0x84))
#define local_58 (*(float *)(lf_frame + 0x88))
#define local_54 (*(float *)(lf_frame + 0x8c))
#define local_50 (*(float *)(lf_frame + 0x90))
#define local_4c (*(float *)(lf_frame + 0x94))
#define local_48 (*(float *)(lf_frame + 0x98))
#define local_44 (*(float *)(lf_frame + 0x9c))
#define local_40 (*(float *)(lf_frame + 0xa0))
#define local_3c (*(float *)(lf_frame + 0xa4))
#define local_38 (*(float *)(lf_frame + 0xa8))
#define local_34 (*(float *)(lf_frame + 0xac))
#define local_30 (*(float *)(lf_frame + 0xb0))
#define local_2c (*(float *)(lf_frame + 0xb4))
#define local_28 (*(float *)(lf_frame + 0xb8))
#define local_24 (*(undefined4 *)(lf_frame + 0xbc))
#define local_20 (*(undefined4 *)(lf_frame + 0xc0))
#define local_1c (*(undefined4 *)(lf_frame + 0xc4))
#define local_18 (*(float *)(lf_frame + 0xc8))
#define local_14 (*(float *)(lf_frame + 0xcc))
#define local_10 (*(float *)(lf_frame + 0xd0))
#define local_c (*(uint *)(lf_frame + 0xd4))
#define g_numofaccounts DAT_0f837b64

/* The bond modes of the joints and the muscles. In a vehicle, the bonds of the bones that do
 * not hold an item leave the solver (mode 0x100). The joint list reads the bone of the muscle
 * with the same index. A muscle is weaker below 25 health and while stun_timer runs. */
static void human_bond_modes(Human *hu, int in_vehicle, int has_item)
{
  int k, health;
  Bond *b;
  float c;

  for (k = 0; k < hu->num_joints; k++) {
    b = &g_bond[hu->joints[k].bond];
    b->mode = 0;
    if (hu->health < 0)
      b->mode = 3;
    if (in_vehicle && (has_item == 0 || 8 < hu->muscles[k].bone))
      b->mode = 0x100;
  }
  health = hu->health;
  for (k = 0; k < hu->num_muscles; k++) {
    b = &g_bond[hu->muscles[k].bond];
    b->mode = 0;
    b->stiffness = 0.0625f;
    b->damping = 0.125f;
    if (health < 25) {
      b->mode = 1;
      b->stiffness = (float)(hu->body_strength * 0.125 * 0.5);
    }
    if (health < 1)
      b->stiffness = 0.0625f;
    if (in_vehicle && (has_item == 0 || 8 < hu->muscles[k].bone))
      b->mode = 0x100;
    if (0 < health && 0 < hu->stun_timer && hu->muscles[k].bone == 1) {
      c = (float)(1.0 - (float)hu->stun_timer / 180.0);
      if (c < 0.0)
        c = 0.0f;
      c = (float)(c * c);
      if (c < 0.1875)
        c = 0.1875f;
      b->stiffness = (float)(c * 0.0625 * c);
      b->damping = (float)(c * 0.125);
    }
  }
}

/* A dead human drops the first item of the first 7 inventory slots. */
static void human_dropitems(Human *hu)
{
  int k;
  uint item;

  for (k = 0; k < 7; k++) {
    if (hu->inventory[k].count <= 0 || (uint)hu->inventory[k].items[0] >= 0x400)
      continue;
    item = (uint)hu->inventory[k].items[0];
    if (g_item[item].parent_item == -1) {
      if (g_item[item].parent_human != -1) {
        item_removehumanlink(item, (uint *)g_item[item].parent_human, g_item[item].parent_slot);
        createevent_updateitem(item);
      }
    } else {
      item_removeitemlink(item, g_item[item].parent_item);
      createevent_updateitem(item);
    }
  }
}

/* A bleeding or dead human loses health once each 256 ticks (the tick of each human differs). */
static void human_bleed(Human *hu, int h)
{
  int health = hu->health;
  vec3 p;

  if (health <= -5 || (hu->bleeding == 0 && 0 < health) || (char)(h * h * h) != (char)g_game.ticks)
    return;
  health = health < 11 ? health - 1 : health - 2;
  hu->health = health;
  if (0 < health) {
    p.x = (float)(g_yaxis[0] * 0.5 + hu->pos.x);
    p.y = (float)(g_yaxis[1] * 0.5 + hu->pos.y);
    p.z = (float)(g_yaxis[2] * 0.5 + hu->pos.z);
    createevent_bullethit(0, 3, (undefined4 *)&p, (undefined4 *)&p);
  }
}

/* The player of a dead human pays the death tax and loses the human. A dead human without a
 * player costs its account 5% of the money above 10000. */
static void human_releaseplayer(Human *hu)
{
  int player = hu->player_id, acc, extra, gametype = g_game.gametype;

  if (player == -1) {
    acc = hu->account;
    if (acc == -1)
      return;
    if (-1 < acc && acc < g_numofaccounts) {
      extra = g_account[acc].money - 10000;
      if (0 < extra)
        g_account[acc].money = g_account[acc].money - (int)(longlong)((double)extra * 0.05000000074505806);
    }
    hu->account = -1;
    return;
  }
  if (g_game.gametype == 4 && (uint)player < 0x100)
    playersellstock(LF_BITS_F(player), g_player[player].stocks);
  playerdeathtax((uint)player);
  if (gametype == 4) {
    if (g_game.team0_manager == hu->player_id)
      g_game.team0_manager = -1;
    if (g_game.team1_manager == hu->player_id)
      g_game.team1_manager = -1;
    if (g_game.team2_manager == hu->player_id)
      g_game.team2_manager = -1;
    if (g_game.team3_manager == hu->player_id)
      g_game.team3_manager = -1;
    if (g_game.team4_manager == hu->player_id)
      g_game.team4_manager = -1;
    g_player[player].team = 7;
  }
  g_player[player].human_id = -1;
  createevent_updateplayer(player);
  hu->player_id = -1;
}

/* despawn_timer: 3600 while a player has the human. A dead one counts down (at least 4 in
 * gametype 3) and lets its player go at 2700. A human without a player counts down. */
static void human_despawn(Human *hu)
{
  int t;

  if (hu->health < -4) {
    if (hu->despawn_timer < 0xa8c)
      human_releaseplayer(hu);
    t = hu->despawn_timer - 1;
    hu->despawn_timer = t;
    if (g_game.gametype == 3 && t < 4)
      hu->despawn_timer = 4;
  } else if (hu->player_id == -1) {
    hu->despawn_timer = hu->despawn_timer - 1;
  } else {
    hu->despawn_timer = 0xe10;
  }
}

/* The type of building i of the city (records of 0x9dc bytes). */
#define BUILDING_TYPE(i) ((&g_city.building_type)[(i) * 0x277])

/* In a building of type 6, a human with a player heals 1 health each 32 ticks and stops the
 * bleeding. A human without a player there goes at once in gametype 4. */
static void human_heal(Human *hu)
{
  int i;

  for (i = 0; i < g_city.num_buildings; i++) {
    if (BUILDING_TYPE(i) != 6 || pointinsidebuilding(i, &hu->pos.x, 0.0f) == 0)
      continue;
    if (hu->player_id == -1) {
      if (g_game.gametype == 4)
        hu->despawn_timer = 0;
    } else if ((g_game.ticks & 0x1f) == 0 && -5 < hu->health && hu->health < 100) {
      hu->health = hu->health + 1;
      hu->bleeding = 0;
    }
  }
}

/* The angles of each bone against its parent bone: the rotation of the bone in the frame of the
 * parent, as three angles (atan2). */
static void human_jointangles(Human *hu)
{
  int k;
  const vec3 *r, *q;
  float m00, m01, m02, m10, m11, m12, m20, m21, m22, a, len, s, c;

  for (k = 1; k < 15; k++) {
    r = hu->bones[k].rot;
    q = hu->bones[g_body[k].parent].rot;
    m00 = (float)(q[0].z * r[0].z + (r[0].y * q[0].y + r[0].x * q[0].x));
    m01 = (float)(r[1].z * q[0].z + (r[1].y * q[0].y + r[1].x * q[0].x));
    m02 = (float)(r[2].z * q[0].z + (r[2].y * q[0].y + r[2].x * q[0].x));
    m10 = (float)(q[1].z * r[0].z + (r[0].x * q[1].x + q[1].y * r[0].y));
    m11 = (float)(q[1].z * r[1].z + (r[1].x * q[1].x + q[1].y * r[1].y));
    m12 = (float)(q[1].z * r[2].z + (r[2].x * q[1].x + q[1].y * r[2].y));
    m20 = (float)(r[0].z * q[2].z + (r[0].x * q[2].x + r[0].y * q[2].y));
    m21 = (float)(r[1].z * q[2].z + (r[1].x * q[2].x + r[1].y * q[2].y));
    m22 = (float)(r[2].z * q[2].z + (r[2].x * q[2].x + r[2].y * q[2].y));
    a = (float)_CIatan2((double)m12, (double)m22);
    len = (float)_CIsqrt((double)(m00 * m00 + m01 * m01));
    hu->bones[k].joint_angles.y = (float)_CIatan2((double)-m02, (double)len);
    s = (float)_CIsin((double)a);
    c = (float)_CIcos((double)a);
    hu->bones[k].joint_angles.z = (float)_CIatan2((double)(s * m20 - c * m10), (double)(c * m11 - m21 * s));
    hu->bones[k].joint_angles.x = a;
  }
}

/* The end of the tick of a human, after the leg and arm angles. vehicle is the vehicle at the
 * start of the tick (-1: on foot). has_item: an item is in slot 0 or 1 of the inventory. */
static void human_finish(int h, int vehicle, int has_item)
{
  Human *hu = &g_human[h];
  int k;

  hu->pos = hu->bones[0].pos;
  human_bond_modes(hu, vehicle != -1, has_item);
  hu->has_physics = 1;
  if (vehicle != -1 && has_item == 0)
    hu->has_physics = 0;
  if (hu->bones[0].pos.y < -32.0)
    hu->health = -100;
  if (hu->health < 1)
    human_dropitems(hu);
  else
    human_itemsimulation(h);
  human_bleed(hu, h);
  if (hu->vehicle_id == -1)
    human_collisionhuman(LF_BITS_F(h));
  human_despawn(hu);
  human_heal(hu);
  if (hu->despawn_timer < 1)
    deletehuman((uint *)h);
  hu->input_buttons_prev = hu->input_buttons;
  for (k = 0; k < 15; k++)
    hu->bones[k].pos_prev = hu->bones[k].pos;
  human_jointangles(hu);
}

/* The muscle strength of a hurt human: below 25 health it drops and wobbles (the phase of the
 * wobble differs for each human), and below 10 it drops further. */
static float human_strength(int h, int health)
{
  float hp = (float)health, q = (float)(hp / 25.0), phase, s;

  phase = (float)((float)((float)((h * h * h + g_game.ticks) & 0x1ff) * 0.001953125) * 6.28318530718);
  s = (float)((double)q * 0.5 + 0.375 + (1.0 - (double)q) * (_CIsin((double)phase) * 0.25));
  if (health < 10)
    s = (float)(((double)(float)(hp / 10.0) * 0.5 + 0.5) * (double)s);
  return s;
}

/* One step of each bone: a dead bone slows down, the bone moves by vel and falls, and it turns
 * by w (an angle of |w| around w). Bone 0 does not go through the level between two ticks. */
static void human_movebones(Human *hu, int in_vehicle)
{
  int k;
  Bone *b;
  vec3 axis;
  float len, inv;

  for (k = 0; k < 15; k++) {
    b = &hu->bones[k];
    b->in_solver = 1.0f;
    if (in_vehicle && (k == 0 || 8 < k))
      b->in_solver = 0.0f;
    if (hu->health < 1) {
      b->vel.x = (float)(b->vel.x * 0.99609375);
      b->vel.y = (float)(b->vel.y * 0.99609375);
      b->vel.z = (float)(b->vel.z * 0.99609375);
      b->ang_momentum.x = (float)(b->ang_momentum.x * 0.99609375);
      b->ang_momentum.y = (float)(b->ang_momentum.y * 0.99609375);
      b->ang_momentum.z = (float)(b->ang_momentum.z * 0.99609375);
    }
    b->pos.x = (float)(b->vel.x + b->pos.x);
    b->pos.y = (float)(b->vel.y + b->pos.y);
    b->pos.z = (float)(b->vel.z + b->pos.z);
    b->vel.y = (float)(b->vel.y - 0.0027222223579883575);
    len = (float)_CIsqrt((double)(float)(b->w.z * b->w.z + (b->w.x * b->w.x + b->w.y * b->w.y)));
    if (1.52587890625E-5 < len) {
      inv = (float)(1.0 / len);
      axis.x = (float)(inv * b->w.x);
      axis.y = (float)(inv * b->w.y);
      axis.z = (float)(inv * b->w.z);
      rotateorientation(&b->rot[0].x, &axis.x, len);
    }
    if (k == 0 && lineintersectlevel(&b->pos_prev.x, &b->pos.x, 1) != 0) {
      b->pos.x = (float)(g_intersect.pos[0] + g_intersect.normal[0] * 0.0625);
      b->pos.y = (float)(g_intersect.normal[1] * 0.0625 + g_intersect.pos[1]);
      b->pos.z = (float)(g_intersect.normal[2] * 0.0625 + g_intersect.pos[2]);
    }
  }
}

/* The start of the tick of a human: the timers, the stance (it moves toward 1, or 0.625 while
 * the crouch button 8 is down, by 1/64), the view angles, the strength and the bone step.
 * Returns 1 when an item is in slot 0 or 1 of the inventory. */
static int human_begin(int h, int vehicle)
{
  Human *hu = &g_human[h];
  float target = 1.0f, stance;

  if (0 < hu->protect_timer)
    hu->protect_timer = hu->protect_timer - 1;
  if ((hu->input_buttons & 8) != 0)
    target = 0.625f;
  stance = target;
  if (target - 0.015625 <= hu->stance) {
    if (target + 0.015625 < hu->stance)
      stance = (float)(hu->stance - 0.015625);
  } else {
    stance = (float)(hu->stance + 0.015625);
  }
  hu->stance = stance;
  if (0 < hu->stun_timer)
    hu->stun_timer = hu->stun_timer - 1;
  hu->on_ground = 0;
  if (vehicle == -1) {
    hu->axis = hu->view_axis;
    hu->pitch = hu->view_pitch;
    hu->vehicle_axis = 0.0f;
    hu->vehicle_pitch = 0.0f;
  } else {
    hu->vehicle_axis = hu->view_axis;
    hu->vehicle_pitch = hu->view_pitch;
  }
  hu->body_strength = 1.0f;
  if (hu->health < 25)
    hu->body_strength = human_strength(h, hu->health);
  human_movebones(hu, vehicle != -1);
  return 0 < hu->inventory[0].count || 0 < hu->inventory[1].count;
}

/* The length of a vector, with the float rounding of the sum. */
static float human_length(float sq)
{
  return (float)_CIsqrt((double)sq);
}

/* A sitting human: bone 0 sits at the seat of the vehicle and turns with it. The driver gives
 * the vehicle its inputs. The other bones hang from their parent bone (g_body), with the head
 * and the arms turned by the view. Button 0x800 held for 30 ticks (more at speed) gets the
 * human out, and each bone moves out of the level walls. seat: the seat at the start of the
 * tick. has_item: the bones 1 to 8 hold an item and stay in the solver. */
static void human_invehicle(int h, int vehicle, int seat, int has_item)
{
  Human *hu = &g_human[h];
  Object *ob = &g_object[vehicle];
  Bone *b0 = &hu->bones[0], *bk;
  const Bone *bp;
  vec3 *r = b0->rot, w;
  float f, len, x, y, z;
  int k, t;
  uint buttons;

  hu->axis = (float)((float)_CIatan2((double)-ob->rot[2].x, (double)ob->rot[2].z) + hu->vehicle_axis);
  hu->pitch = 0.0f;
  if (900 < hu->protect_timer)
    hu->protect_timer = 900;
  if (hu->vehicle_seat == 0) {
    ob->drive_input[0] = hu->input_axes[0];
    ob->drive_buttons = hu->input_buttons;
    ob->drive_input[1] = hu->input_axes[1];
    ob->driver = hu->player_id;
    ob->drive_input[2] = hu->input_axes[2];
    ob->drive_input[3] = hu->input_axes[3];
  }
  /* The seat: x along rot[0] (further out while the human gets out), y along rot[1], z along rot[2]. */
  f = ob->seats[seat].x;
  b0->pos.x = (float)(f * ob->rot[0].x + ob->pos.x);
  b0->pos.y = (float)(ob->rot[0].y * f + ob->pos.y);
  b0->pos.z = (float)(f * ob->rot[0].z + ob->pos.z);
  if (0 < hu->exit_timer) {
    f = (float)(((float)hu->exit_timer * ob->seats[seat].x) / 15.0);
    b0->pos.x = (float)(f * ob->rot[0].x + b0->pos.x);
    b0->pos.y = (float)(ob->rot[0].y * f + b0->pos.y);
    b0->pos.z = (float)(f * ob->rot[0].z + b0->pos.z);
  }
  f = (float)(ob->seats[seat].y + 0.25);
  b0->pos.x = (float)(f * ob->rot[1].x + b0->pos.x);
  b0->pos.y = (float)(ob->rot[1].y * f + b0->pos.y);
  b0->pos.z = (float)(f * ob->rot[1].z + b0->pos.z);
  f = ob->seats[seat].z;
  b0->pos.x = (float)(b0->pos.x + f * ob->rot[2].x);
  b0->pos.y = (float)(ob->rot[2].y * f + b0->pos.y);
  b0->pos.z = (float)(f * ob->rot[2].z + b0->pos.z);
  b0->vel = ob->vel;
  r[0] = ob->rot[0];
  r[1] = ob->rot[1];
  r[2] = ob->rot[2];
  if (0 < hu->exit_timer) {
    /* Make the rotation orthonormal again: rot[2], then rot[1] = rot[0] x rot[2], then rot[0]. */
    len = human_length((float)(r[2].z * r[2].z + (r[2].x * r[2].x + r[2].y * r[2].y)));
    f = 0.0f;
    if (len == 0.0) {
      r[2].x = 0.0f;
      r[2].y = 0.0f;
    } else {
      f = (float)(1.0 / len);
      r[2].x = (float)(f * r[2].x);
      r[2].y = (float)(f * r[2].y);
      f = f * r[2].z;
    }
    r[2].z = f;
    r[1].x = (float)(r[0].z * r[2].y - r[0].y * r[2].z);
    r[1].y = (float)(r[0].x * r[2].z - r[2].x * r[0].z);
    r[1].z = (float)(r[2].x * r[0].y - r[0].x * r[2].y);
    len = human_length((float)(r[1].y * r[1].y + r[1].x * r[1].x + r[1].z * r[1].z));
    f = 0.0f;
    if (len == 0.0) {
      r[1].x = 0.0f;
      r[1].y = 0.0f;
    } else {
      f = (float)(1.0 / len);
      r[1].x = f * r[1].x;
      r[1].y = (float)(r[1].y * f);
      f = f * r[1].z;
    }
    r[1].z = f;
    r[0].x = (float)(r[2].z * r[1].y - r[1].z * r[2].y);
    r[0].y = (float)(r[2].x * r[1].z - r[2].z * r[1].x);
    r[0].z = (float)(r[1].x * r[2].y - r[2].x * r[1].y);
    len = human_length((float)(r[0].z * r[0].z + (r[0].x * r[0].x + r[0].y * r[0].y)));
    f = 0.0f;
    if (len == 0.0) {
      r[0].x = 0.0f;
      r[0].y = 0.0f;
    } else {
      len = (float)(1.0 / len);
      r[0].x = (float)(len * r[0].x);
      r[0].y = (float)(r[0].y * len);
      f = len * r[0].z;
    }
    r[0].z = f;
  }
  /* Bone 0 turns with the vehicle: ang_momentum and w get its angular velocity. */
  getobjectrotation2(&w.x, vehicle, &ob->pos.x);
  b0->ang_momentum = w;
  b0->w = w;
  for (k = 1; k < 15; k++) {
    if (has_item != 0 && k <= 8)
      continue;
    bk = &hu->bones[k];
    bp = &hu->bones[g_body[k].parent];
    bk->rot[0] = bp->rot[0];
    bk->rot[1] = bp->rot[1];
    bk->rot[2] = bp->rot[2];
    if (hu->exit_timer == 0 && (k == 9 || k == 12))
      rotateorientation(&bk->rot[0].x, &bk->rot[0].x, -1.5707963705062866f);
    if (k == 1 || k == 2) {
      rotateorientation(&bk->rot[0].x, &bk->rot[1].x, (float)(hu->view_axis * 0.5));
      rotateorientation(&bk->rot[0].x, &bk->rot[0].x, (float)(hu->view_pitch * 0.5));
    }
    x = g_body[k].pos.x;
    bk->pos.x = (float)(x * bk->rot[0].x + bp->pos.x);
    bk->pos.y = (float)(bk->rot[0].y * x + bp->pos.y);
    bk->pos.z = (float)(x * bk->rot[0].z + bp->pos.z);
    y = g_body[k].pos.y;
    bk->pos.x = (float)(y * bk->rot[1].x + bk->pos.x);
    bk->pos.y = (float)(y * bk->rot[1].y + bk->pos.y);
    bk->pos.z = (float)(y * bk->rot[1].z + bk->pos.z);
    z = g_body[k].pos.z;
    bk->pos.x = (float)(z * bk->rot[2].x + bk->pos.x);
    bk->pos.y = (float)(z * bk->rot[2].y + bk->pos.y);
    bk->pos.z = (float)(z * bk->rot[2].z + bk->pos.z);
    bk->vel = bp->vel;
  }
  hu->last_vehicle = vehicle;
  buttons = hu->input_buttons & 0x800;
  hu->last_vehicle_timer = 100;
  if (buttons == 0)
    hu->exit_timer = 0;
  if (buttons == 0x800 && 0 < hu->exit_timer) {
    t = hu->exit_timer + 1;
    hu->exit_timer = t;
    f = human_length((float)(hu->vel.z * hu->vel.z + (hu->vel.x * hu->vel.x + hu->vel.y * hu->vel.y)));
    if ((int)(longlong)(f * 60.0 + 1.0) + 0x1e <= t) {
      hu->view_axis = hu->axis;
      ob->drive_buttons = 4;
      hu->view_pitch = hu->pitch;
      hu->vehicle_id = -1;
      hu->exit_timer = 0;
      for (k = 0; k < 15; k++) {
        bk = &hu->bones[k];
        if (k != 11 && k != 14 && lineintersectlevel(&ob->pos.x, &bk->pos.x, 0) != 0) {
          bk->pos.x = (float)(g_intersect.pos[0] + g_intersect.normal[0] * 0.0625);
          bk->pos.y = (float)(g_intersect.normal[1] * 0.0625 + g_intersect.pos[1]);
          bk->pos.z = (float)(g_intersect.normal[2] * 0.0625 + g_intersect.pos[2]);
        }
      }
    }
  }
  if (hu->health < 1) {
    ob->drive_buttons = 4;
    hu->view_axis = hu->axis;
    hu->vehicle_id = -1;
    hu->view_pitch = hu->pitch;
  }
  if ((hu->input_buttons & 0x800) != 0 && (hu->input_buttons_prev & 0x800) == 0)
    hu->exit_timer = 1;
}

void __cdecl lf_51060(void)

{
  __declspec(align(16)) uchar lf_frame[0xe0];
  vec3 *pvVar1;
  int *piVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  char cVar6;
  undefined8 uVar7;
  Bond *pBVar8;
  Bone *pBVar9;
  int iVar10;
  uint *puVar11;
  uint *puVar12;
  char *pcVar13;
  int iVar14;
  int iVar15;
  int iVar16;
  int *piVar17;
  uint uVar18;
  float *pfVar19;
  int *piVar20;
  int iVar21;
  Body *pBVar22;
  int iVar23;
  float fVar24;
  bool bVar25;
  float10 fVar26;
  double dVar27;
















































  
  iVar23 = g_timerlist.humansim;
  local_c = DAT_00460d18 ^ (uint)&local_cc;
  iVar21 = g_timerlist.humansim * 0x58;
  pcVar13 = ((char *)SR_VA(0x45f2b0) - 1);
  do {
    pcVar13 = pcVar13 + 1;
    cVar6 = *pcVar13;
    pcVar13[iVar21 + 0x13a50f0] = cVar6;
  } while (cVar6 != '\0');
  g_timer[iVar23].total_lo = 0;
  g_timer[iVar23].total_hi = 0;
  g_timer[iVar23].ms = 0;
  local_cc = &g_timer[iVar23].start_lo;
  uVar7 = rdtsc();
  *local_cc = (int)uVar7;
  g_timer[iVar23].start_hi = (int)((ulonglong)uVar7 >> 0x20);
  piVar17 = (int *)&g_object[0].drive_buttons;
  do {
    *piVar17 = 4;
    piVar17 = piVar17 + 0x208c;
  } while ((int)piVar17 < ((int)SR_VA(0x7a241dc)));
  local_a4 = (uint *)0x0;
  do {
    puVar11 = local_a4;
    fVar24 = 0.0;
    iVar23 = (int)local_a4 * 0x2b64;
    if (g_human[(int)local_a4].active != 0) {
      local_9c = g_human[(int)local_a4].vehicle_id;
      local_98 = LF_BITS_D(CONCAT44((*(uint *)((uchar *)&local_98 + 4)),g_human[(int)local_a4].vehicle_seat));
      LF_SET_BITS(local_a0, human_begin((int)local_a4, local_9c));
      if (local_9c == -1) {
        local_c0 = 0.0;
        if (g_human[(int)puVar11].health < 1) goto LAB_00452144;
        iVar21 = 0;
        local_b0 = 0.0;
        piVar17 = &g_object[0].state;
        local_ac = 0.0;
        local_a8 = 0.0;
        local_bc = 0.0;
        local_b8 = 0.0;
        local_b4 = 0.0;
        LF_COPY(fVar24, g_yaxis[2]);
        LF_COPY(fVar3, g_yaxis[1]);
        LF_COPY(fVar4, g_yaxis[0]);
        do {
          if ((((Object *)(piVar17 + -2))->active != 0) && (*piVar17 != 3)) {
            local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
            local_48 = (float)(g_human[(int)puVar11].bones[0].pos.x + LF_BITS_F(local_cc) * fVar4);
            local_44 = (float)(fVar3 * LF_BITS_F(local_cc) + g_human[(int)puVar11].bones[0].pos.y);
            local_40 = (float)(LF_BITS_F(local_cc) * fVar24 + g_human[(int)puVar11].bones[0].pos.z);
            local_3c = (float)(g_human[(int)puVar11].bones[0].pos.x + fVar4 * 0.125);
            local_38 = (float)(g_human[(int)puVar11].bones[0].pos.y + fVar3 * 0.125);
            local_34 = (float)(fVar24 * 0.125 + g_human[(int)puVar11].bones[0].pos.z);
            iVar14 = lineintersectobject(iVar21,&local_3c,&local_48,0);
            LF_COPY(fVar4, g_yaxis[0]);
            LF_COPY(fVar3, g_yaxis[1]);
            LF_COPY(fVar24, g_yaxis[2]);
            if (iVar14 != 0) {
              local_c0 = 1.401298464324817E-45;
              local_b0 = (float)(g_intersect.pos[0] + 0.0);
              local_ac = (float)(g_intersect.pos[1] + 0.0);
              local_a8 = (float)(g_intersect.pos[2] + 0.0);
              local_bc = (float)(g_intersect.surface_vel.x + 0.0);
              local_b8 = (float)(g_intersect.surface_vel.y + 0.0);
              local_b4 = (float)(g_intersect.surface_vel.z + 0.0);
              goto LAB_00451f2d;
            }
          }
          piVar17 = piVar17 + 0x208c;
          iVar21 = iVar21 + 1;
        } while ((int)piVar17 < ((int)SR_VA(0x7a1e608)));
        local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
        local_48 = (float)(g_human[(int)puVar11].bones[0].pos.x +
                   LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].x);
        local_44 = (float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].y +
                   g_human[(int)puVar11].bones[0].pos.y);
        local_40 = (float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].z +
                   g_human[(int)puVar11].bones[0].pos.z);
        local_3c = g_human[(int)puVar11].bones[0].pos.x;
        local_38 = g_human[(int)puVar11].bones[0].pos.y;
        local_34 = g_human[(int)puVar11].bones[0].pos.z;
        iVar21 = lineintersectlevel(&local_3c,&local_48,1);
        if ((iVar21 == 0) || (g_intersect.normal[1] <= 0.699999988079071)) {
          local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
          local_48 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x +
                     LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].x)) -
                     g_xaxis[0] * 0.1875);
          local_44 = (float)(((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].y +
                     g_human[(int)puVar11].bones[0].pos.y)) - g_xaxis[1] * 0.1875);
          local_40 = (float)(((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].z +
                     g_human[(int)puVar11].bones[0].pos.z)) - g_xaxis[2] * 0.1875);
          local_3c = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x - g_xaxis[0] * 0.1875)) -
                     g_yaxis[0] * 0.375);
          local_38 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.y - g_xaxis[1] * 0.1875)) -
                     g_yaxis[1] * 0.375);
          local_34 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.z - g_xaxis[2] * 0.1875)) -
                     g_yaxis[2] * 0.375);
          iVar21 = lineintersectlevel(&local_3c,&local_48,1);
          if ((iVar21 != 0) && (0.699999988079071 < g_intersect.normal[1])) {
            local_c0 = 1.401298464324817E-45;
            local_b0 = (float)(g_intersect.pos[0] + 0.0);
            local_ac = (float)(g_intersect.pos[1] + 0.0);
            local_a8 = (float)(g_intersect.pos[2] + 0.0);
            local_bc = (float)(g_intersect.surface_vel.x + 0.0);
            local_b8 = (float)(g_intersect.surface_vel.y + 0.0);
            local_b4 = (float)(g_intersect.surface_vel.z + 0.0);
          }
          LF_COPY(fVar24, local_c0);
          local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
          local_48 = (float)((float)(g_human[(int)puVar11].bones[0].pos.x +
                     LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].x) + g_xaxis[0] * 0.1875)
          ;
          local_44 = (float)((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].y +
                     g_human[(int)puVar11].bones[0].pos.y) + g_xaxis[1] * 0.1875);
          local_40 = (float)((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].z +
                     g_human[(int)puVar11].bones[0].pos.z) + g_xaxis[2] * 0.1875);
          local_3c = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x + g_xaxis[0] * 0.1875)) -
                     g_yaxis[0] * 0.375);
          local_38 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.y + g_xaxis[1] * 0.1875)) -
                     g_yaxis[1] * 0.375);
          local_34 = (float)(((float)(g_xaxis[2] * 0.1875 + g_human[(int)puVar11].bones[0].pos.z)) -
                     g_yaxis[2] * 0.375);
          iVar21 = lineintersectlevel(&local_3c,&local_48,1);
          if ((iVar21 != 0) && (0.699999988079071 < g_intersect.normal[1])) {
            LF_SET_BITS(fVar24, (((int)LF_UINT(fVar24)) + 1));
            local_b0 = (float)(g_intersect.pos[0] + local_b0);
            local_ac = (float)(g_intersect.pos[1] + local_ac);
            local_a8 = (float)(g_intersect.pos[2] + local_a8);
            local_bc = (float)(g_intersect.surface_vel.x + local_bc);
            local_b8 = (float)(g_intersect.surface_vel.y + local_b8);
            local_b4 = (float)(g_intersect.surface_vel.z + local_b4);
            local_c0 = fVar24;
          }
          local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
          local_48 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x +
                     LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].x)) -
                     g_zaxis[0] * 0.1875);
          local_44 = (float)(((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].y +
                     g_human[(int)puVar11].bones[0].pos.y)) - g_zaxis[1] * 0.1875);
          local_40 = (float)(((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].z +
                     g_human[(int)puVar11].bones[0].pos.z)) - g_zaxis[2] * 0.1875);
          local_3c = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x - g_zaxis[0] * 0.1875)) -
                     g_yaxis[0] * 0.375);
          local_38 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.y - g_zaxis[1] * 0.1875)) -
                     g_yaxis[1] * 0.375);
          local_34 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.z - g_zaxis[2] * 0.1875)) -
                     g_yaxis[2] * 0.375);
          iVar21 = lineintersectlevel(&local_3c,&local_48,1);
          if ((iVar21 != 0) && (0.699999988079071 < g_intersect.normal[1])) {
            LF_SET_BITS(fVar24, (((int)LF_UINT(fVar24)) + 1));
            local_b0 = (float)(g_intersect.pos[0] + local_b0);
            local_ac = (float)(g_intersect.pos[1] + local_ac);
            local_a8 = (float)(g_intersect.pos[2] + local_a8);
            local_bc = (float)(g_intersect.surface_vel.x + local_bc);
            local_b8 = (float)(g_intersect.surface_vel.y + local_b8);
            local_b4 = (float)(g_intersect.surface_vel.z + local_b4);
            local_c0 = fVar24;
          }
          local_cc = ((int *)LF_BITS_U(((float)(-g_human[(int)puVar11].stance * 1.125))));
          local_48 = (float)((float)(g_human[(int)puVar11].bones[0].pos.x +
                     LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].x) + g_zaxis[0] * 0.1875)
          ;
          local_44 = (float)((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].y +
                     g_human[(int)puVar11].bones[0].pos.y) + g_zaxis[1] * 0.1875);
          local_40 = (float)((float)(LF_BITS_F(local_cc) * g_human[(int)puVar11].bones[0].rot[1].z +
                     g_human[(int)puVar11].bones[0].pos.z) + g_zaxis[2] * 0.1875);
          local_3c = (float)(((float)(g_human[(int)puVar11].bones[0].pos.x + g_zaxis[0] * 0.1875)) -
                     g_yaxis[0] * 0.375);
          local_38 = (float)(((float)(g_human[(int)puVar11].bones[0].pos.y + g_zaxis[1] * 0.1875)) -
                     g_yaxis[1] * 0.375);
          local_34 = (float)(((float)(g_zaxis[2] * 0.1875 + g_human[(int)puVar11].bones[0].pos.z)) -
                     g_yaxis[2] * 0.375);
          iVar21 = lineintersectlevel(&local_3c,&local_48,1);
          if ((iVar21 != 0) && (0.699999988079071 < g_intersect.normal[1])) {
            LF_SET_BITS(fVar24, (((int)LF_UINT(fVar24)) + 1));
            local_b0 = (float)(g_intersect.pos[0] + local_b0);
            local_ac = (float)(g_intersect.pos[1] + local_ac);
            local_a8 = (float)(g_intersect.pos[2] + local_a8);
            local_bc = (float)(g_intersect.surface_vel.x + local_bc);
            local_b8 = (float)(g_intersect.surface_vel.y + local_b8);
            local_b4 = (float)(g_intersect.surface_vel.z + local_b4);
            local_c0 = fVar24;
          }
          if (LF_UINT(fVar24) == 0x0U) {
            local_48 = (float)(g_human[(int)puVar11].bones[0].pos.x - g_yaxis[0] * 0.5);
            local_44 = (float)(g_human[(int)puVar11].bones[0].pos.y - g_yaxis[1] * 0.5);
            local_40 = (float)(g_human[(int)puVar11].bones[0].pos.z - g_yaxis[2] * 0.5);
            local_3c = g_human[(int)puVar11].bones[0].pos.x;
            local_38 = g_human[(int)puVar11].bones[0].pos.y;
            local_34 = g_human[(int)puVar11].bones[0].pos.z;
            iVar21 = lineintersectlevel(&local_3c,&local_48,1);
            if ((iVar21 != 0) && (0.699999988079071 < g_intersect.normal[1])) {
              local_c0 = 1.401298464324817E-45;
              local_b0 = (float)(g_intersect.pos[0] + local_b0);
              local_ac = (float)(g_intersect.pos[1] + local_ac);
              local_a8 = (float)(g_intersect.pos[2] + local_a8);
              local_bc = (float)(g_intersect.surface_vel.x + local_bc);
              local_b8 = (float)(g_intersect.surface_vel.y + local_b8);
              local_b4 = (float)(g_intersect.surface_vel.z + local_b4);
              goto LAB_00451f2d;
            }
          }
          else if (0 < ((int)LF_UINT(fVar24))) goto LAB_00451f2d;
        }
        else {
          local_c0 = 1.401298464324817E-45;
          local_b0 = (float)(g_intersect.pos[0] + 0.0);
          local_ac = (float)(g_intersect.pos[1] + 0.0);
          local_a8 = (float)(g_intersect.pos[2] + 0.0);
          local_bc = (float)(g_intersect.surface_vel.x + 0.0);
          local_b8 = (float)(g_intersect.surface_vel.y + 0.0);
          local_b4 = (float)(g_intersect.surface_vel.z + 0.0);
LAB_00451f2d:
          local_cc = ((int *)LF_BITS_U(((float)(1.0 / (float)((int)LF_UINT(local_c0))))));
          local_b0 = (float)(LF_BITS_F(local_cc) * local_b0);
          local_ac = (float)(local_ac * LF_BITS_F(local_cc));
          local_a8 = (float)(local_a8 * LF_BITS_F(local_cc));
          local_bc = (float)(LF_BITS_F(local_cc) * local_bc);
          local_b8 = (float)(local_b8 * LF_BITS_F(local_cc));
          local_b4 = (float)(LF_BITS_F(local_cc) * local_b4);
          if (((g_human[(int)puVar11].input_buttons & 8) != 0) ||
             (0.8999999761581421 < g_human[(int)puVar11].bones[0].rot[1].y)) {
            fVar24 = ABS(g_human[(int)puVar11].bones[0].rot[1].y);
            local_48 = (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] - local_bc);
            local_44 = (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] - local_b8);
            local_40 = (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] - local_b4);
            local_cc = ((int *)LF_BITS_U(((float)(g_human[(int)puVar11].bones[0].rot[1].z * local_40 +
                              (g_human[(int)puVar11].bones[0].rot[1].y * local_44 +
                              g_human[(int)puVar11].bones[0].rot[1].x * local_48)))));
            local_c8 = (float)((float)(fVar24 * 0.125 * fVar24 *
                       ((float)(g_human[(int)puVar11].stance * 1.125 -
                       ((float)(g_human[(int)puVar11].bones[0].rot[1].z *
                        ((float)(g_human[(int)puVar11].bones[0].pos.z - local_a8)) +
                       (g_human[(int)puVar11].bones[0].rot[1].y *
                       ((float)(g_human[(int)puVar11].bones[0].pos.y - local_ac)) +
                       g_human[(int)puVar11].bones[0].rot[1].x *
                       ((float)(g_human[(int)puVar11].bones[0].pos.x - local_b0)))))))) - LF_BITS_F(local_cc) * 0.75);
            if (local_c8 <= 0.0555555559694767) {
              if (local_c8 <= 0.0) goto LAB_00452109;
            }
            else {
              local_c8 = 0.0555555559694767;
            }
            *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] =
                 (float)(g_human[(int)puVar11].bones[0].rot[1].x * local_c8 +
                 *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18]);
            *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] =
                 (float)(g_human[(int)puVar11].bones[0].rot[1].y * local_c8 +
                 *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c]);
            *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] =
                 (float)(local_c8 * g_human[(int)puVar11].bones[0].rot[1].z +
                 *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20]);
          }
LAB_00452109:
          uVar18 = g_human[(int)puVar11].input_buttons;
          local_c0 = 1.401298464324817E-45;
          g_human[(int)puVar11].on_ground = 1;
          if (((uVar18 & 4) != 0) && ((g_human[(int)puVar11].input_buttons_prev & 4) == 0)) {
            local_c0 = 0.0;
            *(undefined4 *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] = 0x3e2aaaab;
            *(undefined4 *)&g_human[(int)puVar11].bones[1].lf_raw[0x1c] = 0x3e2aaaab;
          }
        }
LAB_00452144:
        iVar21 = 0;
        do {
          if ((iVar21 != 5) && (iVar21 != 8)) {
            local_c8 = 0.125;
            if ((iVar21 == 0xb) || (iVar21 == 0xe)) {
              local_c8 = 0.0625;
            }
            if ((iVar21 == 10) || (iVar21 == 0xd)) {
              local_c8 = 0.0625;
            }
            if ((iVar21 == 4) || (iVar21 == 7)) {
              local_c8 = 0.0625;
            }
            iVar14 = g_human[(int)puVar11].health;
            if ((iVar14 < 1) || (iVar21 != 0)) {
              if ((((iVar21 == 0xb) || (iVar21 == 0xe)) && (0 < iVar14)) ||
                 (((iVar21 == 10 || (iVar21 == 0xd)) && (0 < iVar14)))) goto LAB_0045230e;
            }
            else {
              local_c8 = 0.25;
            }
            pfVar19 = (float *)(iVar21 * 0xe0 + ((int)&g_human[0].bones) + iVar23);
            iVar15 = sphereintersectlevel(pfVar19,local_c8);
            iVar14 = g_numofbonds;
            if (iVar15 != 0) {
              fVar24 = (float)(local_c8 - g_intersect.distance);
              g_bond[g_numofbonds].type = 7;
              g_bond[g_numofbonds].owner = (int)local_a4;
              g_bond[g_numofbonds].bone_a = iVar21;
              g_bond[g_numofbonds].rest_length = fVar24;
              pBVar8 = g_bond + g_numofbonds;
              g_numofbonds = g_numofbonds + 1;
              pBVar8->stiffness = 0.0625;
              g_bond[iVar14].damping = 0.125;
              g_bond[iVar14].mode = 0;
              g_bond[iVar14].normal.x = g_intersect.normal[0];
              g_bond[iVar14].normal.y = g_intersect.normal[1];
              g_bond[iVar14].normal.z = g_intersect.normal[2];
              g_bond[iVar14].friction = 0.5f;
              g_bond[iVar14].arm.x = (float)(g_intersect.pos[0] - *pfVar19);
              g_bond[iVar14].arm.y = (float)(g_intersect.pos[1] - pfVar19[1]);
              g_bond[iVar14].arm.z = (float)(g_intersect.pos[2] - pfVar19[2]);
              fVar24 = *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c];
              fVar3 = *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18];
              fVar4 = *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20];
              local_cc = ((int *)LF_BITS_U(((float)(fVar4 * fVar4 + (fVar3 * fVar3 + fVar24 * fVar24)))));
              dVar27 = _CIsqrt((double)LF_BITS_F(local_cc));
              local_cc = ((int *)LF_BITS_U((float)dVar27));
              if (0.25 < LF_BITS_F(local_cc)) {
                g_human[(int)puVar11].health = g_human[(int)puVar11].health + -0x78;
              }
            }
          }
LAB_0045230e:
          iVar21 = iVar21 + 1;
        } while (iVar21 < 0xf);
        if (LF_UINT(local_c0) != 0x0U) {
          local_30 = 1.0;
          local_2c = 0.0;
          local_28 = 0.0;
          local_24 = 0;
          local_1c = 0;
          local_18 = 0.0;
          local_14 = 0.0;
          local_20 = 0x3f800000;
          local_10 = 1.0;
          rotateorientation(&local_30,g_yaxis,g_human[(int)puVar11].axis);
          fVar3 = -g_human[(int)puVar11].input_axes[3];
          fVar24 = g_human[(int)puVar11].input_axes[1];
          local_48 = (float)(fVar24 * local_30 + ((float)(fVar3 * local_18 + 0.0)));
          local_44 = (float)(local_2c * fVar24 + ((float)(local_14 * fVar3 + 0.0)));
          local_40 = (float)(fVar24 * local_28 + ((float)(fVar3 * local_10 + 0.0)));
          local_cc = ((int *)LF_BITS_U(((float)(local_40 * local_40 + (local_44 * local_44 + local_48 * local_48)))));
          dVar27 = _CIsqrt((double)LF_BITS_F(local_cc));
          local_c4 = ((Bone *)LF_BITS_U((float)dVar27));
          if (LF_BITS_F(local_c4) == 0.0) {
            local_48 = 0.0;
            local_40 = 0.0;
          }
          else {
            local_c4 = ((Bone *)LF_BITS_U(((float)(1.0 / LF_BITS_F(local_c4)))));
            local_48 = LF_BITS_F(local_c4) * local_48;
            local_40 = (float)(LF_BITS_F(local_c4) * local_40);
          }
          local_c0 = 0.15000000596046448;
          if (g_human[(int)puVar11].move_mode == 1) {
            local_c0 = 0.05000000074505806;
          }
          if (g_human[(int)puVar11].move_mode == 2) {
            local_c0 = 0.02500000037252903;
          }
          if ((g_human[(int)puVar11].input_buttons & 8) != 0) {
            local_c0 = (float)(local_c0 * 0.625);
          }
          local_48 = (float)(((float)(local_bc + local_c0 * local_48)) -
                     *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18]);
          local_40 = (float)(((float)(local_b4 + (float)(local_c0 * local_40))) -
                     *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20]);
          local_44 = 0.0;
          local_98 = 0.0;
          local_cc = ((int *)LF_BITS_U(((float)(local_48 * local_48 + 0.0 + local_40 * local_40))));
          dVar27 = _CIsqrt((double)LF_BITS_F(local_cc));
          fVar24 = (float)dVar27;
          local_c8 = (float)(fVar24 / 0.07500000298023224);
          if (0.0 <= local_c8) {
            if (2.0 < local_c8) {
              local_c8 = 2.0;
            }
          }
          else {
            local_c8 = 0.0;
          }
          local_c4 = ((Bone *)LF_BITS_U(((float)((float)(((float)((local_c8 * 0.125 + 0.125) / 60.0)) * 1.25) *
                             g_human[(int)puVar11].body_strength))));
          if (LF_BITS_F(local_c4) < fVar24) {
            fVar3 = (float)(LF_BITS_F(local_c4) / fVar24);
            local_48 = fVar3 * local_48;
            local_44 = (float)(fVar3 * 0.0);
            local_40 = (float)(fVar3 * local_40);
          }
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] =
               (float)(local_48 + *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18]);
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] =
               (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] + local_44);
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] =
               (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] + local_40);
          fVar3 = *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20];
          local_44 = (float)(((float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] - local_b8)) * 0.050000011920928955)
          ;
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] =
               (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] -
               ((float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] - local_bc)) * 0.050000011920928955);
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] =
               (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x1c] - local_44);
          *(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] =
               (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] -
               (float)(((float)(fVar3 - local_b4)) * 0.050000011920928955));
          local_48 = (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x18] - local_bc);
          local_40 = (float)(*(float *)&g_human[(int)puVar11].bones[0].lf_raw[0x20] - local_b4);
          local_cc = ((int *)LF_BITS_U(((float)(local_48 * local_48 + (float)local_98 + local_40 * local_40))));
          local_c8 = fVar24;
          dVar27 = _CIsqrt((double)LF_BITS_F(local_cc));
          fVar24 = (float)((float)dVar27 * 3.14159265359 * 0.5 + g_human[(int)puVar11].walk_phase);
          g_human[(int)puVar11].walk_phase = fVar24;
          if (!NAN(fVar24) && 6.28318530718 < fVar24 != (fVar24 == 6.28318530718)) {
            g_human[(int)puVar11].walk_phase = (float)(fVar24 - 6.28318530718);
          }
        }
        if (g_human[(int)puVar11].last_vehicle_timer < 1) {
          g_human[(int)puVar11].last_vehicle = -1;
        }
        else {
          g_human[(int)puVar11].last_vehicle_timer = g_human[(int)puVar11].last_vehicle_timer + -1;
        }
        local_98 = LF_BITS_D((((ulonglong)LF_BITS_U64(local_98)) & 0xffffffff00000000));
        local_cc = (int *)0x0;
        do {
          if (((((LF_BITS_U(LF_BITS_F(local_cc[(0x1664980 + LF_RELOC / 4)])) != 0x0U) && (LF_BITS_U(LF_BITS_F(local_cc[(0x1664982 + LF_RELOC / 4)])) != 0x3U))
               && ((LF_BITS_F(local_cc[(0x16649a1 + LF_RELOC / 4)]) - 2.0 <= g_human[(int)puVar11].bones[0].pos.x &&
                   ((g_human[(int)puVar11].bones[0].pos.x <= LF_BITS_F(local_cc[(0x16649a4 + LF_RELOC / 4)]) + 2.0 &&
                    (fVar24 = g_human[(int)puVar11].bones[0].pos.z,
                    LF_BITS_F(local_cc[(0x16649a3 + LF_RELOC / 4)]) - 2.0 < fVar24 !=
                    (LF_BITS_F(local_cc[(0x16649a3 + LF_RELOC / 4)]) - 2.0 == fVar24))))))) &&
              (g_human[(int)puVar11].bones[0].pos.z <= LF_BITS_F(local_cc[(0x16649a6 + LF_RELOC / 4)]) + 2.0)) &&
             ((fVar24 = g_human[(int)puVar11].bones[0].pos.y,
              LF_BITS_F(local_cc[(0x16649a2 + LF_RELOC / 4)]) - 2.0 < fVar24 !=
              (LF_BITS_F(local_cc[(0x16649a2 + LF_RELOC / 4)]) - 2.0 == fVar24) &&
              (fVar24 = g_human[(int)puVar11].bones[0].pos.y,
              fVar24 < LF_BITS_F(local_cc[(0x16649a5 + LF_RELOC / 4)]) + 2.0 !=
              (fVar24 == LF_BITS_F(local_cc[(0x16649a5 + LF_RELOC / 4)]) + 2.0))))) {
            local_c4 = (Bone *)&g_human[(int)puVar11].bones[0].lf_raw[0x18];
            local_c0 = 0.0;
            do {
              LF_COPY(fVar24, local_c0);
              if ((((LF_UINT(local_c0) != 0xbU) && (LF_UINT(local_c0) != 0xeU)) &&
                  (LF_UINT(local_c0) != 0xaU)) && (LF_UINT(local_c0) != 0xdU)) {
                local_c8 = 0.25;
                if ((LF_UINT(local_c0) == 0x5U) || (LF_UINT(local_c0) == 0x8U)) {
                  local_c8 = 0.0625;
                }
                iVar16 = sphereintersectobject
                                   ((*(uint *)((uchar *)&local_98 + 0)),(float *)&local_c4[-1].lf_raw[0xc8],local_c8);
                iVar15 = g_numofbonds;
                fVar3 = g_intersect.tri_weights[0];
                iVar14 = g_intersect.tri_particles[2];
                iVar21 = g_intersect.tri_particles[1];
                if (iVar16 != 0) {
                  g_bond[g_numofbonds].type = 4;
                  g_bond[iVar15].owner = (int)local_a4;
                  iVar10 = g_intersect.tri_particles[0];
                  g_bond[iVar15].bone_a = ((int)LF_UINT(fVar24));
                  iVar16 = g_human[(int)puVar11].last_vehicle;
                  g_bond[iVar15].particle_b = iVar10;
                  g_bond[iVar15].particle_c = iVar21;
                  g_bond[iVar15].particle_d = iVar14;
                  g_bond[iVar15].weights[0] = fVar3;
                  fVar4 = g_intersect.tri_weights[1];
                  g_bond[iVar15].weights[1] = g_intersect.tri_weights[1];
                  g_bond[iVar15].weights[2] = g_intersect.tri_weights[2];
                  g_bond[iVar15].rest_length = (float)(local_c8 - g_intersect.distance);
                  g_bond[iVar15].stiffness = 0.0625;
                  if ((*(uint *)((uchar *)&local_98 + 0)) == iVar16) {
                    LF_SET_BITS(local_c8, (100 - g_human[(int)puVar11].last_vehicle_timer));
                    g_bond[iVar15].stiffness = (float)(((float)((int)LF_UINT(local_c8)) * 0.0625) / 100.0);
                  }
                  g_numofbonds = g_numofbonds + 1;
                  g_bond[iVar15].damping = 0.25;
                  fVar24 = g_intersect.normal[0];
                  g_bond[iVar15].mode = 0;
                  g_bond[iVar15].normal.x = fVar24;
                  g_bond[iVar15].normal.y = g_intersect.normal[1];
                  g_bond[iVar15].normal.z = g_intersect.normal[2];
                  g_bond[iVar15].friction = 0.1f;
                  LF_COPY(fVar24, local_c0);
                  if ((((*(uint *)((uchar *)&local_98 + 0)) != g_human[(int)puVar11].last_vehicle) &&
                      (LF_UINT(local_c0) != 0x5U)) &&
                     ((LF_UINT(local_c0) != 0x8U &&
                      ((LF_UINT(local_c0) != 0x4U && (LF_UINT(local_c0) != 0x7U)))))) {
                    local_48 = (float)((&g_particle[0].vel.x)[iVar10 * 0x3c] * fVar3);
                    local_44 = (float)((&g_particle[0].vel.y)[iVar10 * 0x3c] * fVar3);
                    local_40 = (float)((&g_particle[0].vel.z)[iVar10 * 0x3c] * fVar3);
                    scaleaddvectors(&local_48,&local_48,&g_particle[0].vel.x + iVar21 * 0x3c,
                                    fVar4);
                    scaleaddvectors(&local_48,&local_48,&g_particle[0].vel.x + iVar14 * 0x3c,
                                    g_intersect.tri_weights[2]);
                    scaleaddvectors(&local_3c,&local_48,(float *)local_c4,-1.0);
                    local_c0 = (float)(g_intersect.normal[2] * local_34 +
                               (local_38 * g_intersect.normal[1] + g_intersect.normal[0] * local_3c));
                    if ((0.1666666716337204 < local_c0) &&
                       (g_human[(int)puVar11].health = g_human[(int)puVar11].health + -0x78,
                       g_human[(int)puVar11].health + 0x77U < 0x78)) {
                      createevent_sound(g_soundlist.humanhit,&g_human[(int)puVar11].pos.x,lf_u2f(0x3f800000)
                                        ,lf_u2f(0x3f000000));
                      if ((((g_human[(int)puVar11].player_id != -1) &&
                           (g_player[g_human[(int)puVar11].player_id].team == 7)) &&
                          (0 < g_human[(int)puVar11].protect_timer)) &&
                         (LF_BITS_U(LF_BITS_F(local_cc[(0x1664985 + LF_RELOC / 4)])) != 0xffffffffU)) {
                        playerteamkilltax(local_cc[(0x1664985 + LF_RELOC / 4)],100);
                      }
                    }
                  }
                }
              }
              local_c4 = local_c4 + 1;
              LF_SET_BITS(local_c0, (((int)LF_UINT(fVar24)) + 1));
            } while (((int)LF_UINT(local_c0)) < 0xf);
          }
          local_98 = LF_BITS_D(CONCAT44((*(uint *)((uchar *)&local_98 + 4)),(*(uint *)((uchar *)&local_98 + 0)) + 1));
          local_cc = local_cc + 0x208c;
        } while ((int)local_cc < 0x208c000);
        g_human[(int)puVar11].pos.x = g_human[(int)puVar11].bones[0].pos.x;
        g_human[(int)puVar11].pos.y = g_human[(int)puVar11].bones[0].pos.y;
        g_human[(int)puVar11].pos.z = g_human[(int)puVar11].bones[0].pos.z;
        local_c8 = 1.5;
        if (((g_human[(int)puVar11].input_buttons & 0x800) != 0) &&
           ((g_human[(int)puVar11].input_buttons_prev & 0x800) == 0)) {
          local_c0 = 0.0;
          pfVar19 = &g_object[0].pos.y;
          do {
            if ((LF_UINT(pfVar19[-10]) != 0x0U) && (iVar21 = 0, 0 < ((int)LF_UINT(pfVar19[0x2060])))) {
              local_cc = (int *)(pfVar19 + 0x2062);
              do {
                LF_SET_BITS(fVar24, local_cc[-1]);
                LF_SET_BITS(fVar3, *local_cc);
                LF_SET_BITS(fVar4, local_cc[1]);
                local_48 = (float)((float)((float)(fVar24 * pfVar19[5] + ((vec3 *)(pfVar19 + -1))->x) + fVar3 * pfVar19[8]) +
                           fVar4 * pfVar19[0xb]);
                local_44 = (float)(pfVar19[0xc] * fVar4 +
                           ((float)(pfVar19[9] * fVar3 + ((float)(pfVar19[6] * fVar24 + *pfVar19)))));
                local_40 = (float)(fVar4 * pfVar19[0xd] +
                           ((float)(fVar3 * pfVar19[10] + ((float)(fVar24 * pfVar19[7] + pfVar19[1])))));
                local_60 = (float)(local_48 - g_human[(int)puVar11].pos.x);
                local_5c = (float)(local_44 - g_human[(int)puVar11].pos.y);
                local_58 = (float)(local_40 - g_human[(int)puVar11].pos.z);
                local_c4 = ((Bone *)LF_BITS_U(((float)(local_58 * local_58 + (local_5c * local_5c + local_60 * local_60)))))
                ;
                dVar27 = _CIsqrt((double)LF_BITS_F(local_c4));
                local_c4 = ((Bone *)LF_BITS_U((float)dVar27));
                if (LF_BITS_F(local_c4) < local_c8) {
                  piVar17 = &g_human[0].vehicle_id;
                  do {
                    if (((((Human *)(piVar17 + -5))->active != 0) && (LF_BITS_U(LF_BITS_F(*piVar17)) == LF_UINT(local_c0)))
                       && (piVar17[1] == iVar21)) goto LAB_00452eb1;
                    piVar17 = piVar17 + 0xad9;
                  } while ((int)piVar17 < ((int)SR_VA(0x1f38794)));
                  g_human[(int)puVar11].vehicle_id = ((int)LF_UINT(local_c0));
                  g_human[(int)puVar11].vehicle_seat = iVar21;
                  local_54 = (float)(local_48 - g_human[(int)puVar11].pos.x);
                  local_50 = (float)(local_44 - g_human[(int)puVar11].pos.y);
                  local_4c = (float)(local_40 - g_human[(int)puVar11].pos.z);
                  local_c4 = ((Bone *)LF_BITS_U(((float)(local_4c * local_4c +
                                     (local_50 * local_50 + local_54 * local_54)))));
                  dVar27 = _CIsqrt((double)LF_BITS_F(local_c4));
                  local_c8 = (float)dVar27;
                }
LAB_00452eb1:
                local_cc = local_cc + 3;
                iVar21 = iVar21 + 1;
              } while (iVar21 < ((int)LF_UINT(pfVar19[0x2060])));
            }
            LF_SET_BITS(local_c0, (((int)LF_UINT(local_c0)) + 1));
            pfVar19 = pfVar19 + 0x208c;
          } while ((int)pfVar19 < ((int)SR_VA(0x7a1e628)));
        }
      }
      else {
        human_invehicle((int)local_a4, local_9c, (int)(*(uint *)((uchar *)&local_98 + 0)), (int)LF_UINT(local_a0));
      }
      human_calculatelegangles((int)local_a4);
      human_calculatearmangles((int)local_a4);
      human_finish((int)local_a4, local_9c, (int)LF_UINT(local_a0));
    }
    local_a4 = (uint *)((int)local_a4 + 1);
    if (0x1ff < (int)local_a4) {
      stoptimer(g_timerlist.humansim);
      return;
    }
  } while( true );
}
#undef g_human
#undef g_body
#undef DAT_00460d18
#undef g_object
#undef g_game
#undef g_item
#undef g_soundlist
#undef g_particle
#undef DAT_0f837b64
#undef g_city
#undef g_player
#undef g_account
#undef g_intersect
#undef g_timerlist
#undef g_xaxis
#undef g_yaxis
#undef g_zaxis
#undef g_numofbonds
#undef g_bond
#undef g_timer
#undef playersellstock
#undef createevent_updateitem
#undef createevent_updateplayer
#undef item_removehumanlink
#undef item_removeitemlink
#undef pointinsidebuilding
#undef stoptimer
#undef scaleaddvectors
#undef rotateorientation
#undef playerdeathtax
#undef playerteamkilltax
#undef createevent_bullethit
#undef createevent_sound
#undef deletehuman
#undef human_calculatelegangles
#undef human_collisionhuman
#undef getobjectrotation2
#undef lineintersectobject
#undef lineintersectlevel
#undef sphereintersectobject
#undef human_calculatearmangles
#undef human_itemsimulation
#undef sphereintersectlevel
#undef __security_check_cookie
#undef _ftol2
#undef _CIcos
#undef _CIsin
#undef _CIsqrt
#undef _CIatan2
#undef local_cc
#undef local_c8
#undef local_c4
#undef local_c0
#undef local_bc
#undef local_b8
#undef local_b4
#undef local_b0
#undef local_ac
#undef local_a8
#undef local_a4
#undef local_a0
#undef local_9c
#undef local_98
#undef local_90
#undef local_8c
#undef local_88
#undef local_84
#undef local_80
#undef local_7c
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
#undef local_50
#undef local_4c
#undef local_48
#undef local_44
#undef local_40
#undef local_3c
#undef local_38
#undef local_34
#undef local_30
#undef local_2c
#undef local_28
#undef local_24
#undef local_20
#undef local_1c
#undef local_18
#undef local_14
#undef local_10
#undef local_c

