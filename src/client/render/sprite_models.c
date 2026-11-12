/* The pieces that fly and land: shell casings and bits of debris.
 *
 * A piece has a place, a speed, an orientation and a spin. Each tick it falls, spins and
 * moves. When it hits the level it bounces and loses speed. A piece that lies still on flat
 * ground stops: its corners go into a list that stays on the screen, and the piece is free.
 */
#include "render/sprite_models.h"

#include <math.h>
#include <string.h>

#include "client.h"
#include "debug/debug.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_sprites cl_data_sprite_models
#define g_count cl_data_sprite_model_count
#define g_shell_count cl_data_shell_count
#define g_shell_verts cl_data_shell_verts
#define g_debris_count cl_data_debris_count
#define g_debris_verts cl_data_debris_verts
#define g_hit cl_data_level_hit
#define g_models cl_data_model
#define g_modellist sr_data_modellist
#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist
#define g_light_position cl_data_light_position

/* Game functions of other files. */
#define getrandomfloat CL_FN(0x4c5670, double(__cdecl *)(void))
#define lineintersectlevel CL_FN(0x4a0f40, int(__cdecl *)(float *from, float *to, int with_objects))
#define createsound CL_FN(0x44f450, int(__cdecl *)(int sound, const vec3 *pos, const vec3 *vel, float volume, float pitch))

enum {
    MAX_PIECES = 0x4000,
    KIND_SHELL = 0,
    KIND_DEBRIS = 1,
    SHELL_LIMIT = 0x3fe00,
    DEBRIS_LIMIT = 65000,
    GL_VERTEX_ARRAY = 0x8074,
    GL_NORMAL_ARRAY = 0x8075,
    GL_TEXTURE_COORD_ARRAY = 0x8078,
    GL_LIGHT0 = 0x4000,
    GL_POSITION = 0x1203,
    GL_TEXTURE0 = 0x84c0,
    GL_TEXTURE1 = 0x84c1,
    GL_TEXTURE2 = 0x84c2,
    TEXCOORD_OFFSET = 0x0c,
    NORMAL_OFFSET = 0x14
};
#define GRAVITY_BITS 0x3b326750u
#define DRAG_BITS 0x3f7f5c29u
#define SPIN_MIN 1.52587890625E-5
#define HIT_LIFT 0.001953125f
#define SLIDE_KEEP 0.699999988079071
#define BOUNCE_SOUND_MIN 0.01666666753590107
#define FLAT_GROUND 0.7f
#define REST_SPEED 0.008333333767950535
#define SLOT_ACTIVE_TEXTURE 0x115874
/* The up vector of the world: 3 floats. */
#define UP_VECTOR 0x123590

GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))
GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))
GL_IMPORT(glTexCoordPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

static float float_from_bits(unsigned bits)
{
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Start a piece. The kind is 0 for a shell casing and 1 for debris. The speed may be 0. The
 * rotation is three rows of 3 floats. The spin turns about a mix of two rows, at a random rate.
 * The result is the number of the piece, or -1 when all places are used. */
int __cdecl createsprite_model(int kind, const vec3 *pos, const vec3 *vel, const vec3 *rot)
{
    SpriteModel *piece;
    int index;

    if (g_count >= MAX_PIECES)
        return -1;
    piece = &g_sprites[g_count];
    piece->kind = kind;
    piece->pos = *pos;
    if (vel == 0)
        piece->vel.x = piece->vel.y = piece->vel.z = 0.0f;
    else
        piece->vel = *vel;
    piece->rot[0] = rot[0];
    piece->rot[1] = rot[1];
    piece->rot[2] = rot[2];
    piece->gravity = float_from_bits(GRAVITY_BITS);
    piece->drag = float_from_bits(DRAG_BITS);
    scalevector(&piece->spin, &rot[1], (float)((getrandomfloat() * 4.0 + 12.0) / 60.0));
    scaleaddvectors(&piece->spin, &piece->spin, &rot[0], (float)(-(getrandomfloat() * 4.0 + 12.0) / 60.0));
    piece->drag = float_from_bits(DRAG_BITS);
    piece->active = 1;
    index = g_count;
    g_count = g_count + 1;
    return index;
}

/* Free a piece: the last piece takes its place. */
void __cdecl deletesprite_model(int index)
{
    if (-1 < index && index < g_count) {
        g_count = g_count - 1;
        if (index != g_count)
            memcpy(&g_sprites[index], &g_sprites[g_count], sizeof(SpriteModel));
    }
}

/* Count down the life of every piece. A piece with no life left is freed. */
void __cdecl sprite_modeltimetolive(void)
{
    int index = 0;

    while (index < g_count) {
        if (g_sprites[index].life < 0xffff)
            g_sprites[index].life = g_sprites[index].life - 1;
        if (g_sprites[index].life < 1)
            deletesprite_model(index);
        else
            ++index;
    }
}

/* Add the corners of a landed piece to a list of vertices. Each corner takes its place from the
 * model and the orientation of the piece. */
static void add_corners(const SpriteModel *piece, int model, SpriteVertex *list, int *count)
{
    const Model *record = &g_models[model];
    int stride = record->vertex_floats * 4;
    int triangle;
    int corner;

    for (triangle = 0; triangle < record->num_tris; ++triangle) {
        for (corner = 0; corner < 3; ++corner) {
            int vertex = record->tris[triangle].verts[corner];
            const float *source = (const float *)((const char *)record->render_verts + vertex * stride);
            vec3 point;
            vec3 normal;

            scaleaddvectors(&point, &piece->pos, &piece->rot[0], source[0]);
            scaleaddvectors(&point, &point, &piece->rot[1], source[1]);
            scaleaddvectors(&point, &point, &piece->rot[2], source[2]);
            list[*count].pos = point;
            list[*count].texcoord[0] = source[3];
            list[*count].texcoord[1] = source[4];
            normal = *(const vec3 *)(source + 5);
            projectvector(&normal, &normal, piece->rot);
            list[*count].normal = normal;
            *count = *count + 1;
        }
    }
}

/* Move every piece for one tick. */
void __cdecl sprite_model_simulation(void)
{
    int index;

    debug_addataint("spritemodels", g_count);
    for (index = 0; index < g_count; ++index) {
        SpriteModel *piece = &g_sprites[index];
        vec3 before;
        vec3 side;
        float spin_rate;
        float into;

        if (piece->active == 0)
            continue;
        spin_rate = vectorlength(&piece->spin);
        if (SPIN_MIN < spin_rate) {
            scalevector(&before, &piece->spin, (float)(1.0 / spin_rate));
            rotateorientation(piece->rot, &before, spin_rate);
        }
        piece->vel.y = (float)(piece->vel.y - piece->gravity);
        scalevector(&piece->vel, &piece->vel, piece->drag);
        before = piece->pos;
        addvectors(&piece->pos, &piece->pos, &piece->vel);
        if (lineintersectlevel((float *)&before, (float *)&piece->pos, 0) == 0)
            continue;
        scaleaddvectors(&piece->pos, &g_hit.pos, &g_hit.normal, HIT_LIFT);
        into = dotproduct(&piece->vel, &g_hit.normal);
        scaleaddvectors(&side, &piece->vel, &g_hit.normal, -into);
        {
            float side_length = vectorlength(&side);

            if (fabs((double)into) * SLIDE_KEEP < (double)side_length)
                scalevector(&side, &side, (float)(((double)fabs((double)into) * SLIDE_KEEP) / (double)side_length));
        }
        if (piece->kind == KIND_SHELL) {
            if (BOUNCE_SOUND_MIN < -(double)dotproduct(&piece->vel, &g_hit.normal)) {
                unsigned random = (unsigned)os_rand();
                float pitch = (float)((float)(random & 0x7f) / 1024.0 + 1.0);

                createsound(sr_data_soundlist.shell_hit, &piece->pos, &piece->vel,
                            (float)(-(double)dotproduct(&piece->vel, &g_hit.normal) * 0.125 * 0.5 * 60.0), pitch);
            }
        }
        scaleaddvectors(&piece->vel, &piece->vel, &g_hit.normal, (float)(-into * 1.375));
        scaleaddvectors(&piece->vel, &piece->vel, &side, -1.0f);
        scalevector(&piece->vel, &piece->vel, 0.9375f);
        if (piece->kind == KIND_SHELL) {
            const vec3 *up = (const vec3 *)(sr_base + UP_VECTOR);

            crossproduct(&piece->rot[2], up, &piece->rot[0]);
            normalizevector(&piece->rot[2], &piece->rot[2]);
            crossproduct(&piece->rot[0], &piece->rot[2], up);
            normalizevector(&piece->rot[0], &piece->rot[0]);
            piece->rot[1] = *up;
        }
        if (FLAT_GROUND < g_hit.normal.y) {
            if (vectorlength(&piece->vel) < REST_SPEED) {
                piece->active = 0;
                if (piece->kind == KIND_SHELL && g_shell_count < SHELL_LIMIT)
                    add_corners(piece, g_modellist.ammo_b[0], g_shell_verts, &g_shell_count);
                if (piece->kind == KIND_DEBRIS && g_debris_count < DEBRIS_LIMIT)
                    add_corners(piece, g_modellist.sprite[piece->kind], g_debris_verts, &g_debris_count);
            }
        }
    }
}

static void select_texture_unit(uint32_t unit)
{
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(unit);
}

static void bind_texture(int texture)
{
    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
}

/* Draw one list of landed pieces. Mode 0 uses the light and the color and light map textures. */
static void draw_list(const SpriteVertex *list, int count, int texture, int mode)
{
    bind_texture(texture);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (mode == 0) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glColor3f(1.0f, 1.0f, 1.0f);
        select_texture_unit(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        bind_texture(texture);
        select_texture_unit(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.shadow_map);
        glLightfv(GL_LIGHT0, GL_POSITION, g_light_position);
    }
    glVertexPointer(3, GL_FLOAT, sizeof(SpriteVertex), &list[0].pos);
    if (mode == 0) {
        glTexCoordPointer(2, GL_FLOAT, sizeof(SpriteVertex), &list[0].texcoord);
        glNormalPointer(GL_FLOAT, sizeof(SpriteVertex), &list[0].normal);
    }
    glDrawArrays(GL_TRIANGLES, 0, count);
    if (mode == 0) {
        glDisable(GL_TEXTURE_2D);
        select_texture_unit(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        select_texture_unit(GL_TEXTURE0);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
}

/* Draw the landed shell casings and the landed pieces of debris. */
void __cdecl renderspritemodels(int mode)
{
    debug_addataint("numofshellverts", g_shell_count);
    draw_list(g_shell_verts, g_shell_count, g_texturelist.plain_orange, mode);
    debug_addataint("numofdebrisverts", g_debris_count);
    draw_list(g_debris_verts, g_debris_count, g_texturelist.material[3], mode);
}
