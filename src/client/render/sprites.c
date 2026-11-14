/* Particles and glass move under gravity and stop or bounce at the level.
 * Landed particles and glass go into separate lists of four corner shapes.
 * A glass triangle has three moving corners and three springs along its edges.
 */
#include "render/sprites.h"

#include <math.h>
#include <string.h>

#include "client.h"
#include "debug/debug.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/models.h"
#include "vectors.h"

/* Game functions of other files. */
#define setupmodel CL_FN(0x4ac190, void(__cdecl *)(int, int))
typedef int(__cdecl *LevelIntersectFn)(const vec3 *, const vec3 *, int);
typedef void(__cdecl *SpinFn)(vec3 *, const vec3 *, float);
typedef void(__cdecl *RandomVectorFn)(vec3 *, float, float);
typedef float(__cdecl *PointDistanceFn)(const vec3 *, const vec3 *, const vec3 *);

enum { SPRITE_LIMIT = 16384, DECAL_LIMIT = 16000, WINDOW_GRID = 8,
       MAX_ITEMS = 1024, HEAD_BONE = 2, LEFT_HAND_BUTTON = 0x10,
       LIFE_FOREVER = 0xffff, TRIANGLE_MOVING_LIFE = 0xe808 };
#define SLIDE_KEEP 0.699999988079071
#define NORMAL_AXIS_LIMIT 0.7070000171661377
#define GLASS_SPIN_MIN 0.004166666883975267

/* Align two rows to a surface normal. */
static void surface_rows(vec3 rows[3])
{
    const vec3 *axis = fabs((double)rows[2].y) >= NORMAL_AXIS_LIMIT ? &sr_data_xaxis : &sr_data_yaxis;

    crossproduct(&rows[0], &rows[2], axis);
    normalizevector(&rows[0], &rows[0]);
    crossproduct(&rows[1], &rows[0], &rows[2]);
    normalizevector(&rows[1], &rows[1]);
}

/* Reduce speed across the surface and reflect speed into the surface. */
static void bounce(vec3 *velocity, float drag)
{
    const vec3 *normal = &cl_data_level_hit.normal;
    vec3 side;
    float into = (float)dotproduct(velocity, normal);
    float length;

    scaleaddvectors(&side, velocity, normal, -into);
    length = (float)vectorlength(&side);
    if (fabs((double)into) * SLIDE_KEEP < (double)length)
        scalevector(&side, &side, (float)(fabs((double)into) * SLIDE_KEEP / (double)length));
    scaleaddvectors(velocity, velocity, normal, (float)(-into * 1.125));
    scaleaddvectors(velocity, velocity, &side, -1.0f);
    scalevector(velocity, velocity, drag);
}

/* Store a corner of a landed particle after two rounded vector operations. */
static vec3 decal_corner(const ClientSprite *sprite, float x, float y)
{
    vec3 corner;

    scaleaddvectors(&corner, &sprite->pos, &sprite->rot[0], x);
    scaleaddvectors(&corner, &corner, &sprite->rot[1], y);
    return corner;
}

/* Add four corners of a landed particle or glass shard to its list. */
static void land_sprite(ClientSprite *sprite)
{
    int corner;
    float size = sprite->size;
    static const float signs[4][2] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};
    static const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

    sprite->life = 0;
    if (sprite->kind == SPRITE_GLASS) {
        if (cl_data_glass_vertex_count >= DECAL_LIMIT)
            return;
        rotateorientation(sprite->rot, &sprite->rot[2],
                          (float)(((float)((unsigned)os_rand() & 0xff) * 3.14159265359) / 256.0));
        for (corner = 0; corner < 4; ++corner) {
            GlassVertex *vertex = &cl_data_glass_vertices[cl_data_glass_vertex_count++];

            vertex->pos = decal_corner(sprite, signs[corner][0] * size, signs[corner][1] * size);
            vertex->normal = sprite->rot[2];
        }
    } else {
        if (cl_data_sprite_decal_count >= DECAL_LIMIT)
            return;
        for (corner = 0; corner < 4; ++corner) {
            SpriteDecalVertex *vertex = &cl_data_sprite_decals[cl_data_sprite_decal_count++];

            vertex->pos = decal_corner(sprite, signs[corner][0] * size, signs[corner][1] * size);
            memcpy(vertex->texcoord, uv[corner], sizeof(vertex->texcoord));
        }
    }
}

/* Move the three corners, then apply the springs with the speeds before the spring pass. */
static void simulate_triangle(ClientSprite *sprite, LevelIntersectFn intersect)
{
    vec3 speeds[3];
    int corner;

    for (corner = 0; corner < 3; ++corner) {
        vec3 before = sprite->rot[corner];

        sprite->corner_vel[corner].y = (float)(sprite->corner_vel[corner].y - sprite->gravity);
        addvectors(&sprite->rot[corner], &sprite->rot[corner], &sprite->corner_vel[corner]);
        if (intersect(&before, &sprite->rot[corner], 0)) {
            scaleaddvectors(&sprite->rot[corner], &cl_data_level_hit.pos, &cl_data_level_hit.normal, 0.001953125f);
            bounce(&sprite->corner_vel[corner], 0.875f);
        }
    }
    memcpy(speeds, sprite->corner_vel, sizeof(speeds));
    for (corner = 0; corner < 3; ++corner) {
        int next = (corner + 1) % 3;
        vec3 direction;
        float force;

        subtractvectors(&direction, &sprite->rot[next], &sprite->rot[corner]);
        force = (float)vectorlength(&direction);
        normalizevector(&direction, &direction);
        force = (float)((float)(force - ((const float *)&sprite->spin)[corner]) * 0.125);
        force = (float)((double)force - (double)dotproduct(&speeds[corner], &direction) * 0.25);
        force = (float)((double)dotproduct(&speeds[next], &direction) * 0.25 + (double)force);
        scaleaddvectors(&sprite->corner_vel[corner], &sprite->corner_vel[corner], &direction, force);
        scaleaddvectors(&sprite->corner_vel[next], &sprite->corner_vel[next], &direction, -force);
    }
}

/* Move each particle for one tick. Surface hits stop stains and upward facing glass. */
void __cdecl spritesimulation(void)
{
    int index;
    LevelIntersectFn intersect = CL_FN(0x4a0f40, LevelIntersectFn);
    SpinFn spin = CL_FN(0x4e4f10, SpinFn);

    debug_addataint("sprites", cl_data_sprite_count);
    for (index = 0; index < cl_data_sprite_count; ++index) {
        ClientSprite *sprite = &cl_data_sprites[index];
        vec3 before;

        if (sprite->kind == SPRITE_SPARK) {
            sprite->vel.y = (float)(sprite->vel.y - sprite->gravity);
            scalevector(&sprite->vel, &sprite->vel, sprite->drag);
            before = sprite->pos;
            addvectors(&sprite->pos, &sprite->pos, &sprite->vel);
            if (intersect(&before, &sprite->pos, 0)) {
                scaleaddvectors(&sprite->pos, &cl_data_level_hit.pos, &cl_data_level_hit.normal, 0.015625f);
                bounce(&sprite->vel, 0.9375f);
            }
        }
        if (sprite->kind == SPRITE_TRIANGLE && sprite->life > TRIANGLE_MOVING_LIFE)
            simulate_triangle(sprite, intersect);
        if ((sprite->kind != SPRITE_STAIN && sprite->kind != SPRITE_GLASS) || !(0.0f < sprite->drag))
            continue;
        sprite->vel.y = (float)(sprite->vel.y - sprite->gravity);
        scalevector(&sprite->vel, &sprite->vel, sprite->drag);
        before = sprite->pos;
        addvectors(&sprite->pos, &sprite->pos, &sprite->vel);
        if (sprite->kind == SPRITE_GLASS && GLASS_SPIN_MIN < sprite->spin_rate)
            spin(sprite->rot, &sprite->spin, sprite->spin_rate);
        if (!intersect(&before, &sprite->pos, 0)) {
            if (sprite->kind != SPRITE_GLASS)
                memcpy(sprite->rot, cl_data_camera_rotation, sizeof(sprite->rot));
            continue;
        }
        scaleaddvectors(&sprite->pos, &cl_data_level_hit.pos, &cl_data_level_hit.normal, 0.015625f);
        if (sprite->kind == SPRITE_GLASS && !(0.0f < cl_data_level_hit.normal.y)) {
            bounce(&sprite->vel, 0.9375f);
            continue;
        }
        memset(&sprite->vel, 0, sizeof(sprite->vel));
        sprite->drag = 0.0f;
        if (sprite->kind == SPRITE_STAIN) {
            sprite->size = (float)((float)((unsigned)os_rand() & 0x3f) / 1024.0 + 0.0625);
            sprite->size = (float)(sprite->size * 0.5);
        }
        sprite->rot[2] = cl_data_level_hit.normal;
        surface_rows(sprite->rot);
        land_sprite(sprite);
    }
}

/* Split the last window with a matching face into a nine by nine grid of shards. */
void __cdecl createwindowsprites(int object, int face, const vec3 *hit, const vec3 *impulse)
{
    const Object *body = &sr_data_object[object];
    const Model *model = &cl_data_model[sr_data_modellist.bullet[body->type]];
    int window = 0;
    int index;
    int row;
    int column;
    vec3 corners[4];
    vec3 rows[3];
    vec3 left;
    vec3 right;
    vec3 left_step;
    vec3 right_step;
    RandomVectorFn random_vector = CL_FN(0x4c5700, RandomVectorFn);

    for (index = 0; index < model->num_windows; ++index)
        if (model->windows[index].face == face)
            window = index;
    for (index = 0; index < 4; ++index) {
        projectvector(&corners[index], &model->windows[window].corners[index], body->rot);
        addvectors(&corners[index], &corners[index], &body->pos);
    }
    generatetrinormal(&rows[2], &corners[0], &corners[1], &corners[2]);
    surface_rows(rows);
    left = corners[0];
    right = corners[1];
    subtractvectors(&left_step, &corners[3], &corners[0]);
    scalevector(&left_step, &left_step, 0.125f);
    subtractvectors(&right_step, &corners[2], &corners[1]);
    scalevector(&right_step, &right_step, 0.125f);
    for (row = 0; row <= WINDOW_GRID; ++row) {
        vec3 position = left;
        vec3 step;

        subtractvectors(&step, &right, &left);
        scalevector(&step, &step, 0.125f);
        for (column = 0; column <= WINDOW_GRID; ++column) {
            if (cl_data_sprite_count < SPRITE_LIMIT) {
                ClientSprite *sprite = &cl_data_sprites[cl_data_sprite_count];
                vec3 away;
                float strength;

                sprite->kind = SPRITE_GLASS;
                sprite->pos = position;
                strength = (float)(1.0 - (double)vectordistance(&position, hit) * 4.0);
                if (strength < 0.03125f)
                    strength = 0.03125f;
                subtractvectors(&away, &position, hit);
                random_vector(&sprite->vel, 0.008333333767950535f, 0.0020833334419876337f);
                scaleaddvectors(&sprite->vel, &sprite->vel, &away, 0.03125f);
                addvectors(&sprite->vel, &sprite->vel, &body->vel);
                scaleaddvectors(&sprite->vel, &sprite->vel, impulse, (float)(strength * 0.0625));
                sprite->life = LIFE_FOREVER;
                sprite->size = 0.0625f;
                sprite->gravity = 0.0027222223579883575f;
                sprite->drag = 0.9975000023841858f;
                memcpy(sprite->rot, rows, sizeof(rows));
                sprite->spin.x = (float)((float)((unsigned)os_rand() & 0xff) / 512.0 - 0.25);
                sprite->spin.y = (float)((float)((unsigned)os_rand() & 0xff) / 512.0 - 0.25);
                sprite->spin.z = (float)((float)((unsigned)os_rand() & 0xff) / 512.0 - 0.25);
                normalizevector(&sprite->spin, &sprite->spin);
                sprite->spin_rate = (float)(((float)((unsigned)os_rand() & 0xff) / 512.0 - 0.25) * 0.5);
                ++cl_data_sprite_count;
            }
            addvectors(&position, &position, &step);
        }
        addvectors(&left, &left, &left_step);
        addvectors(&right, &right, &right_step);
    }
}

/* Build a small box for kind 1. Other kinds produce an empty model. */
void __cdecl createspritemodel(int model, int kind)
{
    int triangle;

    cl_data_modeltemp.num_verts = 0;
    cl_data_modeltemp.num_tris = 0;
    cl_data_modeltemp.num_quads = 0;
    cl_data_modeltemp.num_lines = 0;
    if (kind == 1) {
        const vec3 center = {0, 0, 0};

        addboxtomodel(&center, 0.02f, 0.02f, 0.02f);
    }
    for (triangle = 0; triangle < cl_data_modeltemp.num_tris; ++triangle)
        memset(&cl_data_modeltemp.tri_texcoords[triangle], 0, sizeof(TriTexCoords));
    setupmodel(model, 0);
}

/* Select the nearest free item within half a unit of the human's reach segment. */
void __cdecl selectnearitem(void)
{
    const Player *player;
    const Human *human;
    const Bone *head;
    int hand;
    int item;
    vec3 end;
    float nearest = 0.5f;
    PointDistanceFn distance_to_line;

    if (cl_data_view_player == -1)
        return;
    player = &sr_data_player[cl_data_view_player];
    if (player->human_id == -1)
        return;
    human = &sr_data_human[player->human_id];
    hand = (player->input_buttons & LEFT_HAND_BUTTON) != 0;
    cl_data_selected_item = -1;
    if (human->inventory[hand].count >= 1)
        return;
    head = &human->bones[HEAD_BONE];
    scaleaddvectors(&end, &head->pos, &head->rot[2], -1.5f);
    distance_to_line = CL_FN(0x4fd150, PointDistanceFn);
    for (item = 0; item < MAX_ITEMS; ++item) {
        const Item *candidate = &sr_data_item[item];
        float distance;

        if (!candidate->active || candidate->parent_human != -1 || candidate->parent_item != -1)
            continue;
        distance = distance_to_line(&head->pos, &end, &candidate->pos);
        if (distance < nearest) {
            cl_data_selected_item = item;
            nearest = distance_to_line(&head->pos, &end, &candidate->pos);
        }
    }
}
