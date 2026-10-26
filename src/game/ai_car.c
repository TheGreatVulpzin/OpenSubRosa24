/* The driving of a car by the AI.
 *
 * A bot car follows a list of road points (Player.waypoints): the road, the side of the road,
 * the lane and the turn at the next crossing. ai_playercar steers toward a point on the lane
 * of the current road, picks a speed from the distance to the next crossing and to the car
 * ahead, and writes the result in the input axes of the player. ai_car_nextwaypoint adds the
 * next road point at a crossing.
 *
 * The input axes: 0 and 2 select the gear (forward or back), 1 is the steering, 3 is the
 * throttle and the brake.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include "ai_car.h"

#include <math.h>

#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define sr_city sr_data_city

/* A value in the range -1 to 1 (steering or throttle). A value that is not a number stays as it is. */
static float clamp_unit(float v)
{
  if (v < -1.0f)
    return -1.0f;
  if (v > 1.0f)
    return 1.0f;
  return v;
}

/* Which side of a crossing a road is on: 0 for the first side, 1 to 3 for the others. The last
 * side that matches wins. */
static int crossing_side(const CityIntersection *c, int road)
{
  int d = 0;

  if (road == c->roads[1])
    d = 1;
  if (road == c->roads[2])
    d = 2;
  if (road == c->roads[3])
    d = 3;
  return d;
}

/* Add the next road point after the current one. The car comes to the crossing at the end of
 * the current road and takes one of its other roads. */
void ai_car_nextwaypoint(int player)
{
  Player *p = &sr_players[player];
  AiWaypoint *wp = &p->waypoints[p->ai_waypoint];
  AiWaypoint *next = &p->waypoints[p->ai_waypoint + 1];
  int road = wp->road;
  const CityIntersection *c = &sr_city.intersections[sr_city.roads[road].ends[wp->dir ^ 1]];
  int d = crossing_side(c, road);
  int u, tries, x4;

  /* At the goal the car picks a new goal: a random crossing. */
  if (c->x == p->ai_dest_x && c->z == p->ai_dest_z) {
    int j = 0;

    if (sr_city.num_intersections > 0)
      j = os_rand() % sr_city.num_intersections;
    p->ai_dest_x = sr_city.intersections[j].x;
    p->ai_dest_z = sr_city.intersections[j].z;
  }

  /* The side to leave by: toward the goal. The crossing 4 splits the city in two parts. */
  x4 = sr_city.intersections[4].x;
  if (c->z == p->ai_dest_z)
    u = (c->x < p->ai_dest_x) ? 0 : 2;
  else if (c->x < x4 && d != 0)
    u = 0;
  else if (c->x <= x4 || d == 2)
    u = (p->ai_dest_z <= c->z) ? 3 : 1;
  else
    u = 2;

  /* A side with no road, or the road of the car, gets a random side. After 512 tries the last
   * side is used with no test. */
  for (tries = 0; tries < 0x200; tries++) {
    if (c->roads[u] != road && c->roads[u] != -1)
      break;
    u = os_rand() & 3;
  }

  next->road = c->roads[u];
  next->dir = (u >= 2);
  next->lane = os_rand() & 1;
  next->turn = 0;
  if (u == ((d + 1) & 3))
    next->turn = -1;
  if (u == ((d - 1) & 3))
    next->turn = 1;
}

/* One active object `oj` near the car `o`. A chase car in the same lane changes lane. An object
 * ahead that drives the same way lowers the wanted speed `target`. */
static void near_object(Player *p, const Object *o, const Object *oj, const vec3 *dc, float *target)
{
  vec3 rr;
  float sd, ah, len2, cand;

  rr.x = (float)(oj->pos.x - o->pos.x);
  rr.z = (float)(oj->pos.z - o->pos.z);
  if (!(fabs(rr.x) < 64.0) || !(fabs(rr.z) < 64.0))
    return;
  rr.y = (float)(oj->pos.y - o->pos.y);
  sd = (float)fabs((float)((rr.y * o->rot[0].y + o->rot[0].x * rr.x) + o->rot[0].z * rr.z));
  ah = (float)((dc->y * rr.y + dc->x * rr.x) + dc->z * rr.z);
  if (p->ai_chase && p->lane_cooldown < 1 && ah > 2.0f && sd < 2.0f) {
    int *lane = &p->waypoints[p->ai_waypoint].lane;

    if ((os_rand() & 1) == 0)
      *lane = (*lane >= 2) ? *lane - 1 : *lane + 1;
    else
      *lane = (*lane < -1) ? *lane + 1 : *lane - 1;
    if (*lane > 2)
      *lane = 2;
    if (*lane < -2)
      *lane = -2;
    p->lane_cooldown = 60;
  }
  if (ah > 2.0f && sd < 2.0f && dotproduct(&o->rot[2], &oj->rot[2]) > 0.0f) {
    len2 = (float)(vectordistance(&o->pos, &oj->pos) - 8.0);
    if (len2 < 0.0f)
      len2 = 0.0f;
    if (p->ai_chase && len2 < 16.0f)
      len2 = 16.0f;
    cand = (float)(vectorlength(&oj->vel) + (float)((len2 * 0.5) / 60.0));
    if (cand < *target)
      *target = cand;
  }
}

/* Drive the car `object` of the bot `player` for one tick. */
void ai_playercar(int player, int object)
{
  Object *objs = sr_objects;
  Object *o = &objs[object];
  const Object *oj;
  Player *p = &sr_players[player];
  const vec3 *r1 = &o->rot[1];
  const vec3 *r2 = &o->rot[2];
  AiWaypoint *wp;
  const CityRoad *rd;
  CityIntersection *a, *b, *c;
  vec3 from, p3, dc, v, dd, e, cross;
  float to_x, to_z, lat, lateral, off, s, shift, m, tt, steer, fwd;
  float q_x, q_z, ahead, f, k, target, lim, vd, thr, sp;
  double fd;
  int g, side, next_isec, j, st;

  /* The gear follows the rpm. */
  if (o->gear > 1 && (double)o->engine_rpm < 4.363323129986111)
    o->gear--;
  if (o->gear < 4 && 8.726646259972222 < (double)o->engine_rpm)
    o->gear++;
  g = o->gear;
  p->input_axes[0] = (g == 1 || g == 2) ? 0.0f : 1.0f;
  p->input_axes[2] = (g == 1 || g == 3) ? -1.0f : 1.0f;

  wp = &p->waypoints[p->ai_waypoint];
  if (wp->road == -1)
    return;
  rd = &sr_city.roads[wp->road];
  a = &sr_city.intersections[rd->ends[0]];
  b = &sr_city.intersections[rd->ends[1]];

  /* The road runs from `from` to `to`. A road point with dir 1 runs the other way. */
  if (wp->dir == 0) {
    from = a->pos;
    to_x = b->pos.x;
    to_z = b->pos.z;
  } else {
    from = b->pos;
    to_x = a->pos.x;
    to_z = a->pos.z;
  }

  /* How far the car may be from the line of the road: less the farther it is from the start. */
  lat = p->ai_chase ? 32.0f : 16.0f;
  dd.x = (float)(from.x - o->pos.x);
  dd.y = (float)(from.y - o->pos.y);
  dd.z = (float)(from.z - o->pos.z);
  s = (float)((dd.y * rd->side.y + rd->side.x * dd.x) + dd.z * rd->side.z);
  lat = (float)(lat - fabsf(s) * 0.75);
  if (lat < 0.0f)
    lat = 0.0f;

  /* The lane: the offset from the middle of the road, and the direction of the car. */
  if (wp->dir == 0) {
    off = (float)((double)wp->lane * 4.0 + 2.0);
    dc = rd->dir;
    lateral = lat;
  } else {
    off = (float)(-2.0 - (double)wp->lane * 4.0);
    dc.x = -rd->dir.x;
    dc.y = -rd->dir.y;
    dc.z = -rd->dir.z;
    lateral = -lat;
  }
  from.x = (float)(from.x + off * rd->side.x);
  from.y = (float)(from.y + off * rd->side.y);
  from.z = (float)(from.z + off * rd->side.z);
  to_x = (float)(off * rd->side.x + to_x);
  to_z = (float)(off * rd->side.z + to_z);
  p3.x = (float)(lateral * rd->dir.x + o->pos.x);
  p3.y = (float)(lateral * rd->dir.y + o->pos.y);
  p3.z = (float)(lateral * rd->dir.z + o->pos.z);

  /* The end of the road is moved by the reach of the crossing. */
  if (rd->kind == 0)
    shift = (wp->dir == 0) ? -(float)b->extent[3] : (float)a->extent[1];
  else
    shift = (wp->dir == 0) ? -(float)b->extent[2] : (float)a->extent[0];
  m = (float)(shift * 6.0);
  to_x = (float)(m * rd->dir.x + to_x);
  to_z = (float)(m * rd->dir.z + to_z);

  /* The aim point: the car projected on the lane line. */
  e.x = (float)(p3.x - from.x);
  e.y = (float)(p3.y - from.y);
  e.z = (float)(p3.z - from.z);
  tt = (float)((dc.y * e.y + dc.x * e.x) + dc.z * e.z);
  p3.x = (float)(tt * dc.x + from.x);
  p3.y = (float)(tt * dc.y + from.y);
  p3.z = (float)(tt * dc.z + from.z);

  /* Steering: the side of the aim direction in the frame of the car. */
  v.x = (float)(p3.x - o->pos.x);
  v.y = (float)(p3.y - o->pos.y);
  v.z = (float)(p3.z - o->pos.z);
  normalizevector(&v, &v);
  cross.x = (float)(r2->y * v.z - v.y * r2->z);
  cross.y = (float)(v.x * r2->z - v.z * r2->x);
  cross.z = (float)(v.y * r2->x - r2->y * v.x);
  steer = (float)((r1->y * cross.y + cross.x * r1->x) + r1->z * cross.z);
  fwd = (float)((v.y * r2->y + v.x * r2->x) + v.z * r2->z);
  if (fwd > 0.707f && steer < 0.0f)
    steer = -1.0f;
  else if (fwd > 0.707f && steer > 0.0f)
    steer = 1.0f;
  else
    steer = clamp_unit(steer);
  p->input_axes[1] = (float)(steer + p->input_axes[1]);

  /* The distance to the crossing at the end of the road, along the lane. */
  q_x = (float)(to_x - o->pos.x);
  q_z = (float)(to_z - o->pos.z);
  next_isec = rd->ends[wp->dir ^ 1];
  c = &sr_city.intersections[next_isec];
  ahead = (float)((q_x * dc.x + dc.y * 0.0) + q_z * dc.z);
  side = crossing_side(c, wp->road);
  if (next_isec == 4 && side == 3)
    ahead = (float)(ahead - 14.0);

  /* At the crossing the car takes the next road point (a chase car ignores the lights). */
  if (ahead < 6.0f && (c->red[side] == 0 || p->ai_chase != 0)) {
    const AiWaypoint *nx = &p->waypoints[p->ai_waypoint + 1];

    wp->road = nx->road;
    wp->dir = nx->dir;
    wp->lane = nx->lane;
    ai_car_nextwaypoint(player);
  }

  /* A car that waited at a light starts when it is far from the crossing. */
  j = p->wait_isec;
  if (j != -1 && p->wait_light == sr_city.intersections[j].light &&
      vectordistance(&o->pos, &sr_city.intersections[j].pos) > 32.0f) {
    sr_city.intersections[j].light++;
    sr_city.intersections[j].light_timer = 0;
    p->wait_isec = -1;
  }

  /* The speed: slower when the car does not look along the lane, and at the crossing. */
  v.x = (float)(p3.x - o->pos.x);
  v.y = (float)(p3.y - o->pos.y);
  v.z = (float)(p3.z - o->pos.z);
  normalizevector(&v, &v);
  f = (float)((v.y * r2->y + v.x * r2->x) + v.z * r2->z);
  k = (float)(fabsf(f) * fabsf(f) * 0.75 + 0.25);
  {
    const AiWaypoint *nx = &p->waypoints[p->ai_waypoint + 1];

    target = (float)(k * 20.0 / 60.0);
    if (p->ai_chase) {
      target = (float)(k * 30.0 / 60.0);
      if (nx->turn == 0)
        target = (float)(k * 40.0 / 60.0);
    }
    lim = (float)((ahead * 0.5) / 60.0);
    if (ahead < 2.0f)
      lim = 0.0f;
    if (c->red[side] == 0)
      lim = (nx->turn == 0) ? target : (float)(((ahead + 16.0) * 0.5) / 60.0);
  }
  if (p->ai_chase)
    lim = (float)(lim * 4.0);
  if (lim < target)
    target = lim;
  if (p->lane_cooldown > 0)
    p->lane_cooldown--;

  /* The other objects close in front: change lane (a chase car) or keep behind them. Most
   * objects are not active, so the loop only tests the flag and leaves the work to a helper. */
  for (j = 0, oj = objs; j < 1024; j++, oj++)
    if (j != object && oj->active)
      near_object(p, o, oj, &dc, &target);

  /* The throttle: three times the speed along the car plus the wanted speed. A car that does not move for 3 seconds
   * backs out for 2 seconds and then tries again. */
  vd = (float)((o->vel.x * r2->x + r2->y * o->vel.y) + o->vel.z * r2->z);
  fd = (double)vd + target;
  thr = (float)(fd * 3.0);
  st = p->stuck_ticks;
  if (st < 0xb4) {
    sp = (float)x87_sqrt((float)((o->vel.x * o->vel.x + o->vel.y * o->vel.y) + o->vel.z * o->vel.z));
    if (sp < 0.03333333507180214f && 0.06666667014360428 < fd)
      p->stuck_ticks = st + 1;
    else
      p->stuck_ticks = 0;
  } else {
    thr = 1.0f;
    p->input_axes[2] = 1.0f;
    p->input_axes[0] = 2.0f;
    p->input_axes[1] = 0.0f;
    if (st >= 0x118)
      thr = -1.0f;
    p->stuck_ticks = (st + 1 < 0x140) ? st + 1 : 0;
  }
  if (thr < 0.0f)
    thr = (float)(thr * 4.0);
  p->input_axes[3] = clamp_unit(thr);
}
