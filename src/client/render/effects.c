/* Tire strips follow the ground under each wheel and form a bounded ring.
 * Muzzle flashes use a small sphere mesh, stretched along the gun barrel.
 * Checkpoints draw both faces of a vertical strip along one side of a box.
 */
#include "render/sprites.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"
#include "../../game/collide.h"

/* Game functions of other files. */
typedef int(__cdecl *LevelIntersectFn)(const vec3 *, const vec3 *, int);

enum { MAX_OBJECTS = 1024, MAX_ITEMS = 1024, TIREMARK_MASK = 0x3fff,
       DEPTH_LEQUAL = 0x203, CHECKPOINTS_OFFSET = 0x25978, FLASH_MESH = 0 };

/* Test x and z against the inclusive low edges and exclusive high edges. */
int pointinsidecheckpoint(const vec3 *position, int checkpoint)
{
    const RenderCheckpoint *boxes = (const RenderCheckpoint *)((const char *)&sr_data_game + CHECKPOINTS_OFFSET);
    const RenderCheckpoint *box = &boxes[checkpoint];
    return box->low.x <= position->x && !(box->high.x <= position->x) &&
           box->low.z <= position->z && !(box->high.z <= position->z);
}


/* Add a tire strip when slip exceeds 1/32. Save the wheel state for the next tick. */
void __cdecl tiremark_simulation(void)
{
    int object;
    int wheel;
    LevelIntersectFn intersect = lineintersectlevel;

    for (object = 0; object < MAX_OBJECTS; ++object) {
        Object *body = &sr_data_object[object];

        if (!body->active)
            continue;
        for (wheel = 0; wheel < body->num_wheels; ++wheel) {
            Wheel *tire = &body->wheels[wheel];

            if (0.03125f < tire->slip) {
                Tiremark *mark = &cl_data_tiremarks[sr_data_tiremarkend];
                vec3 direction;
                vec3 side;
                int corner;

                subtractvectors(&direction, &tire->pos, &tire->mark_pos);
                normalizevector(&direction, &direction);
                side.x = -direction.z;
                side.y = direction.y;
                side.z = direction.x;
                for (corner = 0; corner < 4; ++corner) {
                    TiremarkVertex *vertex = &mark->corners[corner];
                    const vec3 *center = corner < 2 ? &tire->pos : &tire->mark_pos;
                    vec3 below;
                    float offset = (corner == 0 || corner == 3) ? -0.0625f : 0.0625f;

                    scaleaddvectors(&vertex->pos, center, &side, offset);
                    scaleaddvectors(&below, &vertex->pos, &sr_data_yaxis, -1.0f);
                    if (intersect(&vertex->pos, &below, 0))
                        vertex->pos.y = (float)((*(LevelHit *)&sr_data_intersect).pos.y + 0.03125);
                    vertex->alpha = (float)((corner < 2 ? tire->slip : tire->mark_slip) * 8.0);
                }
                sr_data_tiremarkend = (sr_data_tiremarkend + 1) & TIREMARK_MASK;
                if (sr_data_tiremarkstart == sr_data_tiremarkend)
                    sr_data_tiremarkstart = (sr_data_tiremarkstart + 1) & TIREMARK_MASK;
            }
            tire->mark_pos = tire->pos;
            tire->mark_slip = tire->slip;
        }
    }
}

/* Draw the tire ring with one alpha for each corner. */
void __cdecl tiremark_render(void)
{
    uint32_t strip;
    int corner;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(0, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(DEPTH_LEQUAL);
    glBegin(GL_QUADS);
    for (strip = sr_data_tiremarkstart; strip != sr_data_tiremarkend; strip = (strip + 1) & TIREMARK_MASK)
        for (corner = 0; corner < 4; ++corner) {
            const TiremarkVertex *vertex = &cl_data_tiremarks[strip].corners[corner];

            glColor4f(0, 0, 0, vertex->alpha);
            glVertex3fv(&vertex->pos.x);
        }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/* Draw the flash while an active item's countdown is positive. */
void __cdecl rendermuzzleflashes(void)
{
    const SphereLevel *mesh = &cl_data_sphere[FLASH_MESH];
    int item;

    glDepthMask(0);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, 1);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 0.75f, 0.125f, 0.5f);
    for (item = 0; item < MAX_ITEMS; ++item) {
        const Item *gun = &sr_data_item[item];
        float size;
        int triangle;
        int corner;

        if (!gun->active || gun->countdown <= 0)
            continue;
        size = (float)((float)gun->countdown * 0.125 * 0.125 + 0.125);
        for (triangle = 0; triangle < mesh->num_tris; ++triangle)
            for (corner = 0; corner < 3; ++corner) {
                int index = mesh->tris[triangle].verts[corner];
                const vec3 *source = &mesh->verts[index];
                vec3 position;

                scaleaddvectors(&position, &gun->pos, &gun->rot[0], (float)(source->x * size));
                scaleaddvectors(&position, &position, &gun->rot[1], (float)(source->y * size));
                scaleaddvectors(&position, &position, &gun->rot[2], (float)(source->z * size * (index == 1 ? 1.5 : 3.0)));
                scaleaddvectors(&position, &position, &gun->rot[2], (float)(sr_data_itemtype[gun->type].muzzle.z - size));
                glVertex3fv(&position.x);
            }
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(1);
}

/* Draw each enabled checkpoint as a strip 1.5 units high. */
void __cdecl rendercheckpoints(void)
{
    const RenderCheckpoint *checkpoints = (const RenderCheckpoint *)((const char *)&sr_data_game + CHECKPOINTS_OFFSET);
    int player = cl_data_audio_camera.player == -1 ? 0 : cl_data_audio_camera.player;
    int index;
    vec3 first;
    vec3 second;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (index = 0; index < sr_data_game.round_goal; ++index) {
        const RenderCheckpoint *checkpoint = &checkpoints[index];
        vec3 bottom_first;
        vec3 bottom_second;
        float width;

        if (sr_data_game.round_ticks <= 0 && index != 0)
            continue;
        if (sr_data_game.round_ticks < 1)
            glColor3f(0.5f, 0, 0);
        else if (index < sr_data_player[player].round_points)
            glColor3f(0, 0.5f, 0);
        else
            glColor3f(0.75f, 0.75f, 0.75f);
        /* An invalid side uses the corners of the preceding checkpoint. */
        if (checkpoint->side == 0) {
            first = (vec3){checkpoint->low.x, checkpoint->high.y, checkpoint->low.z};
            second = (vec3){checkpoint->low.x, checkpoint->high.y, checkpoint->high.z};
        }
        if (checkpoint->side == 1) {
            first = (vec3){checkpoint->low.x, checkpoint->high.y, checkpoint->high.z};
            second = checkpoint->high;
        }
        if (checkpoint->side == 2) {
            first = checkpoint->high;
            second = (vec3){checkpoint->high.x, checkpoint->high.y, checkpoint->low.z};
        }
        if (checkpoint->side == 3) {
            first = (vec3){checkpoint->high.x, checkpoint->high.y, checkpoint->low.z};
            second = (vec3){checkpoint->low.x, checkpoint->high.y, checkpoint->low.z};
        }
        bottom_second = second;
        bottom_second.y = (float)(bottom_second.y - 1.5);
        bottom_first = first;
        bottom_first.y = (float)(bottom_first.y - 1.5);
        width = (float)vectordistance(&second, &first);
        glTexCoord2f(0, 1);
        glVertex3fv(&first.x);
        glTexCoord2f(width, 1);
        glVertex3fv(&second.x);
        glTexCoord2f(width, 0);
        glVertex3fv(&bottom_second.x);
        glTexCoord2f(0, 0);
        glVertex3fv(&bottom_first.x);
        glTexCoord2f(width, 1);
        glVertex3fv(&second.x);
        glTexCoord2f(0, 1);
        glVertex3fv(&first.x);
        glTexCoord2f(0, 0);
        glVertex3fv(&bottom_first.x);
        glTexCoord2f(width, 0);
        glVertex3fv(&bottom_second.x);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}
