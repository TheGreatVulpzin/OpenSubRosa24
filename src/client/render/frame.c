/* The frame pass sets the view, draws depth and shadows, then draws the scene in color.
 * The color pass draws opaque geometry, the sky, alpha surfaces, car windows and effects.
 * The world debug view adds wire boxes, contact points, bonds and wheel shapes.
 */
#include "render/frame.h"
#include "client.h"
#include "gl_slots.h"
#include "render/atmosphere.h"
#include "render/debug_draw.h"
#include "render/frame_list.h"
#include "render/landscape_draw.h"
#include "render/lists.h"
#include "render/models_draw.h"
#include "render/sectorlist.h"
#include "render/sectors.h"
#include "render/sky_light.h"
#include "render/sprite_models.h"
#include "render/sprites.h"
#include "render/text.h"
#include "setup/timers.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "util.h"
#include "vectors.h"
#include "render/view.h"
#include "render/debug_cylinder.h"

enum { CULL_FACE = 0xb44, DEPTH_LEQUAL = 0x203, FRONT_CW = 0x900, LIGHT0 = 0x4000,
       LIGHT_AMBIENT = 0x1200, LIGHT_DIFFUSE = 0x1201, LIGHT_POSITION = 0x1203,
       FACE_FRONT = 0x404, FACE_BACK = 0x405, MODE_LINE = 0x1b01, MODE_FILL = 0x1b02,
       LINES = 1, TRIANGLES = 4, QUADS = 7,
       ARRAY_POSITION = 0x8074, ARRAY_NORMAL = 0x8075, ARRAY_COLOR = 0x8076, ARRAY_TEXCOORD = 0x8078,
       TEXTURE0 = 0x84c0, TEXTURE1 = 0x84c1, FRAMEBUFFER = 0x8d40, COLOR_ATTACHMENT = 0x8ce0,
       CLEAR_COLOR = 0x4000, CLEAR_COLOR_DEPTH_STENCIL = 0x4500,
       ONE = 1, SRC_ALPHA = 0x302, ONE_MINUS_SRC_ALPHA = 0x303,
       PROGRAM_SECTOR = 0, PROGRAM_REFLECT = 2, PROGRAM_MODEL = 3,
       UNIFORM_CAMERA = 1, UNIFORM_WORLD = 4, UNIFORM_LIGHT = 5,
       VIEW_WIDTH = 0x400, VIEW_HEIGHT = 0x280, NUM_HUMANS = 0x200, NUM_ITEMS = 0x400,
       NUM_OBJECTS = 0x400, NUM_CITYOBJECTS = 0x400, NUM_BONES = 15, NUM_TEAMS = 5,
       OBJECT_HELICOPTER = 11, CITYOBJECT_ELEVATOR = 2, BOND_PARTICLES = 0, BOND_BONES = 9,
       GAMETYPE_RACE = 2, KEY_SHADOWS = 14, KEY_NAMES = 20 };

GL_IMPORT(glDepthFunc, (uint32_t test), (test))
GL_IMPORT(glClearColor, (float r, float g, float b, float a), (r, g, b, a))
GL_IMPORT(glClearStencil, (int value), (value))
GL_IMPORT(glStencilMask, (uint32_t mask), (mask))
GL_IMPORT(glClear, (uint32_t mask), (mask))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))
GL_IMPORT(glColor3f, (float r, float g, float b), (r, g, b))
GL_IMPORT(glColor3fv, (const float *color), (color))
GL_IMPORT(glPolygonMode, (uint32_t face, uint32_t mode), (face, mode))
GL_IMPORT(glFrontFace, (uint32_t face), (face))
GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glNormal3fv, (const float *normal), (normal))
GL_IMPORT(glTexCoord2f, (float u, float v), (u, v))
GL_IMPORT(glVertex3fv, (const float *position), (position))
GL_IMPORT(glPushMatrix, (void), ())
GL_IMPORT(glPopMatrix, (void), ())

typedef void(__stdcall *UseProgramFn)(uint32_t program);
typedef void(__stdcall *ActiveTextureFn)(uint32_t unit);
typedef void(__stdcall *Uniform3Fn)(int location, int count, const float *value);
typedef void(__stdcall *BindFramebufferFn)(uint32_t target, uint32_t framebuffer);
typedef void(__stdcall *AttachTextureFn)(uint32_t, uint32_t, uint32_t, uint32_t, int);

static void use_program(uint32_t program)
{
    cl_glUseProgramObjectARB(program);
}

static void active_texture(uint32_t unit)
{
    cl_glActiveTextureARB(unit);
}

static void bind_framebuffer(uint32_t framebuffer)
{
    cl_glBindFramebuffer(FRAMEBUFFER, framebuffer);
}

static void bind_image(int image)
{
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[image].gl_name);
}

/* Set the perspective view with the camera of the frame. */
static void frame_view(void)
{
    setupperspectiveviewport(0, 0, VIEW_WIDTH, VIEW_HEIGHT, cl_data_view_near, cl_data_view_far, cl_data_view_fov);
    setupviewpoint(&cl_data_audio_camera.pos, cl_data_audio_camera.rot);
}

/* Reset the profiler timers of the frame passes. */
static void reset_timers(void)
{
    const TimerListClient *timers = &sr_data_timerlist;
    resettimer(timers->raycast, "raycast");
    resettimer(timers->buildframelist, "framesetup");
    resettimer(timers->renderdepth, "renderdepth");
    resettimer(timers->rendersectors, "rendersectors");
    resettimer(timers->rendermodels, "rendermodels");
    resettimer(timers->renderinterior, "renderinterior");
    resettimer(timers->renderinteriorwindows, "renderinteriorwindows");
    resettimer(timers->renderinteriorshadow, "renderinteriorshadow");
    resettimer(timers->rendermodelshadow, "rendermodelshadow");
    resettimer(timers->renderlandscape, "renderlandscape");
    resettimer(timers->rendersectorshadow, "rendersectorshadow");
    resettimer(timers->renderatmosphere, "renderatmosphere");
    resettimer(timers->renderalpha, "renderalpha");
}

/* Set the camera values that the passes of the frame read. */
static void setup_view(void)
{
    cl_data_viewport_x = 0.0f;
    cl_data_viewport_y = 0.0f;
    cl_data_viewport_width = (float)cl_data_render_width;
    cl_data_viewport_height = (float)cl_data_render_height;
    cl_data_view_near = 0.0625f;
    cl_data_view_far = 16384.0f;
    cl_data_view_fov = cl_data_audio_camera.projection;
    cl_data_view_side_right = (vec3){0.7071f, 0.0f, 0.7071f};
    projectvector(&cl_data_view_side_right, &cl_data_view_side_right, cl_data_audio_camera.rot);
    cl_data_view_side_left = (vec3){-0.7071f, 0.0f, 0.7071f};
    projectvector(&cl_data_view_side_left, &cl_data_view_side_left, cl_data_audio_camera.rot);
    cl_data_world_position = (vec3){0.0f, 637.1f, 0.0f};
    scaleaddvectors(&cl_data_world_position, &cl_data_world_position, &cl_data_audio_camera.pos, 0.0001f);
}

/* Point light 0 at the sun. The tilt is the angle from straight up. */
static void setup_light(void)
{
    const ClientGameView *game = (const ClientGameView *)&sr_data_game;
    float *light = cl_data_light_position;
    light[0] = (float)(x87_sin(game->sun_tilt) * -x87_cos(game->sun_heading));
    light[1] = (float)x87_cos(game->sun_tilt);
    light[2] = (float)(x87_sin(game->sun_tilt) * -x87_sin(game->sun_heading));
    light[3] = 0.0f;
    glLightfv(LIGHT0, LIGHT_POSITION, light);
    getsuncolors();
    getskycolor(cl_data_sky_color);
    glLightfv(LIGHT0, LIGHT_AMBIENT, cl_data_light_ambient);
    glLightfv(LIGHT0, LIGHT_DIFFUSE, cl_data_light_diffuse);
}

/* Draw the opaque geometry: sectors, landscape, interiors, models and loose pieces. */
static void draw_opaque(void)
{
    static const float white[3] = {1.0f, 1.0f, 1.0f};
    int textures[2];
    int i;
    render_sectorlist(0);
    renderlandscape(0);
    use_program(cl_data_programs[PROGRAM_SECTOR].program);
    glEnableClientState(ARRAY_POSITION);
    glEnableClientState(ARRAY_NORMAL);
    glEnableClientState(ARRAY_TEXCOORD);
    glEnableClientState(ARRAY_COLOR);
    active_texture(TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    bind_image(sr_data_texturelist.shadow_map);
    active_texture(TEXTURE0);
    for (i = 0; i < cl_data_sector_count; ++i)
        render_sector_interior(cl_data_sector_list[i], 0);
    active_texture(TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    active_texture(TEXTURE0);
    glDisableClientState(ARRAY_COLOR);
    glDisableClientState(ARRAY_TEXCOORD);
    glDisableClientState(ARRAY_NORMAL);
    glDisableClientState(ARRAY_POSITION);
    use_program(0);
    use_program(cl_data_programs[PROGRAM_MODEL].program);
    rendermodellist(0);
    for (i = 0; i < sr_data_numofsprite_models; ++i) {
        const SpriteModel *piece = &cl_data_sprite_models[i];
        if (!piece->active)
            continue;
        if (piece->kind == 0) {
            textures[0] = textures[1] = sr_data_texturelist.plain_orange;
            rendermodel(sr_data_modellist.bullet_flat[0], &piece->pos, piece->rot, 0, textures, white);
        }
        if (piece->kind == 1) {
            textures[0] = textures[1] = sr_data_texturelist.material[3];
            rendermodel(sr_data_modellist.sprite[piece->kind], &piece->pos, piece->rot, 0, textures, white);
        }
    }
    renderspritemodels(0);
    rendertrackmodel(0);
    use_program(0);
}

/* Draw the car windows from the far one to the near one. */
static void draw_windows(void)
{
    int i;
    for (i = 0; i < cl_data_window_quad_count; ++i)
        sr_data_radix.keys[i] = cl_ftoi(16777216.0 - cl_data_window_quads[i].distance * 256.0);
    sr_data_radix.count = cl_data_window_quad_count;
    csradixsort();
    glBegin(QUADS);
    for (i = 0; i < cl_data_window_quad_count; ++i) {
        const WindowQuad *quad = &cl_data_window_quads[sr_data_radix.order[i]];
        const float *ambient = cl_data_light_ambient;
        glNormal3fv(&quad->normal.x);
        glColor4f((float)(ambient[0] * 0.5), (float)(ambient[1] * 0.5), (float)(ambient[2] * 0.5),
                  quad->facing ? 0.25f : 0.1875f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(&quad->corners[0].x);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(&quad->corners[1].x);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(&quad->corners[2].x);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(&quad->corners[3].x);
    }
    glEnd();
}

/* Draw the floor buttons on each elevator panel. The current floor is white. */
static void draw_elevator_panels(void)
{
    int i, row, column;
    for (i = 0; i < NUM_CITYOBJECTS; ++i) {
        const CityObjectRec *lift = &sr_data_cityobject[i];
        vec3 panel;
        if (!lift->active || lift->type != CITYOBJECT_ELEVATOR)
            continue;
        scaleaddvectors(&panel, &lift->pos, &lift->rot[2], -1.435f);
        scaleaddvectors(&panel, &panel, &lift->rot[0], 0.875f);
        scaleaddvectors(&panel, &panel, &lift->rot[1], -0.625f);
        glPushMatrix();
        setupobjectrender(&panel, lift->rot);
        for (row = 0; row < 4; ++row)
            for (column = 0; column < 8; ++column) {
                int floor = column + row * 8;
                float x = (float)((float)column * 0.125 * 0.75 + 0.046875);
                float y = (float)((float)row * 0.25 * 0.75 + 0.09375);
                float shade = lift->floor == floor ? 1.0f : 0.5f;
                if (floor >= lift->num_floors)
                    continue;
                drawtext("/i", x, y, 0.0703125f, 0x81, shade, shade, shade, 1.0f, floor + 1);
            }
        glPopMatrix();
    }
}

/* Draw the boxes of the human bones and of the team areas as wire frames. */
static void draw_debug_boxes(void)
{
    const ClientGameView *game = (const ClientGameView *)&sr_data_game;
    int i, bone;
    for (i = 0; i < NUM_HUMANS; ++i) {
        const Human *human = &sr_data_human[i];
        if (!human->active)
            continue;
        for (bone = 0; bone < NUM_BONES; ++bone)
            debug_renderbox(&human->bones[bone].pos, human->bones[bone].rot, sr_data_body[bone].half_size.x,
                            sr_data_body[bone].half_size.y, sr_data_body[bone].half_size.z);
    }
    for (i = 0; i < NUM_TEAMS; ++i) {
        debug_renderboundingbox(&game->teams[i].base_min.x, &game->teams[i].base_max.x);
        debug_renderboundingbox(&game->teams[i].safe_min.x, &game->teams[i].safe_max.x);
    }
}

/* Draw a small sphere at each corner of the box of each item. */
static void draw_debug_items(void)
{
    int i, corner;
    for (i = 0; i < NUM_ITEMS; ++i) {
        const Item *item = &sr_data_item[i];
        const vec3 *size;
        if (!item->active)
            continue;
        size = &sr_data_itemtype[item->type].size;
        glColor3f(0.5f, 0.5f, 0.0f);
        if (item->has_physics)
            glColor3f(0.0f, 1.0f, 1.0f);
        for (corner = 0; corner < 8; ++corner) {
            vec3 point;
            int side = corner & 3;
            scaleaddvectors(&point, &item->pos, &item->rot[0], side == 0 || side == 3 ? -size->x : size->x);
            scaleaddvectors(&point, &point, &item->rot[1], corner < 4 ? -size->y : size->y);
            scaleaddvectors(&point, &point, &item->rot[2], side < 2 ? -size->z : size->z);
            rendersphere(&point, 0.0625f, 1);
        }
    }
}

/* Draw the triangles of the train tracks. */
static void draw_debug_tracks(void)
{
    int i, tri;
    glBegin(TRIANGLES);
    for (i = 0; i < sr_data_numoftracks; ++i) {
        const TrackPiece *piece = &sr_data_track[i];
        for (tri = 0; tri < piece->num_tris; ++tri) {
            glVertex3fv(&piece->verts[piece->tris[tri][0]].x);
            glVertex3fv(&piece->verts[piece->tris[tri][1]].x);
            glVertex3fv(&piece->verts[piece->tris[tri][2]].x);
        }
    }
    glEnd();
}

/* Add the lines of the bone joints, the particle pairs and the debug lines. */
static void draw_debug_lines(void)
{
    int i;
    for (i = 0; i < sr_data_numofbonds; ++i) {
        const Bond *bond = &sr_data_bond[i];
        if (bond->type == BOND_BONES) {
            const Human *human = &sr_data_human[bond->owner];
            glVertex3fv(&human->bones[bond->bone_a].pos.x);
            glVertex3fv(&human->bones[bond->target].pos.x);
        }
        if (bond->type == BOND_PARTICLES) {
            glColor3f(1.0f, 1.0f, 0.0f);
            glVertex3fv(&sr_data_particle[bond->particle_a].pos.x);
            glVertex3fv(&sr_data_particle[bond->particle_b].pos.x);
        }
    }
    glColor3f(1.0f, 0.0f, 1.0f);
    for (i = 0; i < sr_data_debug.num_lines; ++i) {
        glVertex3fv(&sr_data_debug.lines[i].from.x);
        glVertex3fv(&sr_data_debug.lines[i].to.x);
    }
}

/* Draw a cylinder at each wheel and at each wheel body of each vehicle. */
static void draw_debug_wheels(void)
{
    int i, wheel;
    for (i = 0; i < NUM_OBJECTS; ++i) {
        const Object *object = &sr_data_object[i];
        if (!object->active)
            continue;
        if (object->type == OBJECT_HELICOPTER)
            glColor3f(1.0f, 1.0f, 0.0f);
        for (wheel = 0; wheel < object->num_wheels; ++wheel) {
            vec3 pos = object->wheels[wheel].pos;
            vec3 rot[3];
            rot[0] = object->rot[0];
            rot[1] = object->rot[1];
            rot[2] = object->rot[2];
            rotateorientation(rot, &rot[1], object->wheels[wheel].steer);
            rotateorientation(rot, &rot[0], object->wheels[wheel].angle);
            debug_rendercylinder(&pos, rot);
        }
        for (wheel = 0; wheel < object->num_wheel_bodies; ++wheel) {
            vec3 pos = object->wheel_bodies[wheel].pos;
            vec3 rot[3];
            rot[0] = object->wheel_bodies[wheel].rot[0];
            rot[1] = object->wheel_bodies[wheel].rot[1];
            rot[2] = object->wheel_bodies[wheel].rot[2];
            debug_rendercylinder(&pos, rot);
        }
    }
}

/* Draw the name of each player near the camera above the head. */
static void draw_name_tags(void)
{
    int i;
    for (i = 0; i < NUM_HUMANS; ++i) {
        const Human *human = &sr_data_human[i];
        vec3 point;
        if (!human->active || human->player_id == -1)
            continue;
        if (!(vectordistance(&cl_data_audio_camera.pos, &human->pos) < 32.0f))
            continue;
        glPushMatrix();
        point = human->pos;
        point.y = (float)(point.y + 1.25);
        setupobjectrender(&point, cl_data_audio_camera.rot);
        drawtext(sr_data_player[human->player_id].name, 0.0f, 0.0f, 0.5f, 0xc1, 1.0f, 1.0f, 1.0f, 1.0f);
        glPopMatrix();
    }
}

/* Draw a sphere at each race checkpoint. A checkpoint that the viewed player has passed is grey. */
static void draw_race_checkpoints(void)
{
    const ClientGameView *game = (const ClientGameView *)&sr_data_game;
    int i;
    for (i = 0; i < game->num_checkpoints; ++i) {
        if (cl_data_audio_camera.player == -1 || !(sr_data_player[cl_data_audio_camera.player].checkpoint_mask & (1u << (i & 31))))
            glColor4f(0.0f, 1.0f, 0.0f, 0.5f);
        else
            glColor4f(0.25f, 0.25f, 0.25f, 0.5f);
        rendersphere(&game->checkpoints[i], 1.0f, 2);
    }
}

/* Set both faces to the same polygon mode. */
static void polygon_mode(uint32_t mode)
{
    glPolygonMode(FACE_FRONT, mode);
    glPolygonMode(FACE_BACK, mode);
}

/* Draw the sun as a large sphere in the color of the diffuse light. */
static void draw_sun(void)
{
    vec3 point;
    vec3 color;
    scaleaddvectors(&point, &cl_data_audio_camera.pos, (const vec3 *)cl_data_light_position, 2048.0f);
    normalizevector(&color, (const vec3 *)cl_data_light_diffuse);
    glColor3fv(&color.x);
    rendersphere(&point, 32.0f, 3);
}

/* Draw the reflecting alpha surfaces of the sectors. */
static void draw_sector_alpha(void)
{
    const ProgramObject *program = &cl_data_programs[PROGRAM_REFLECT];
    int i;
    use_program(program->program);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_CAMERA], 1, &cl_data_audio_camera.pos.x);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_WORLD], 1, &cl_data_world_position.x);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_LIGHT], 1, cl_data_light_position);
    glEnableClientState(ARRAY_POSITION);
    glEnableClientState(ARRAY_NORMAL);
    glEnableClientState(ARRAY_TEXCOORD);
    for (i = 0; i < cl_data_sector_count; ++i)
        render_sector_alpha(cl_data_sector_list[i], 0);
    glDisableClientState(ARRAY_TEXCOORD);
    glDisableClientState(ARRAY_NORMAL);
    glDisableClientState(ARRAY_POSITION);
    use_program(0);
}

/* Draw the alpha pass: sun, tire marks, reflections, effects, windows and the selected item. */
static void draw_alpha(void)
{
    static const float white[3] = {1.0f, 1.0f, 1.0f};
    renderframeatmosphere(0.625f);
    starttimer(sr_data_timerlist.renderalpha);
    frame_view();
    glEnable(GL_BLEND);
    glBlendFunc(ONE, ONE);
    glDisable(GL_TEXTURE_2D);
    draw_sun();
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    glDepthMask(0);
    tiremark_render();
    glEnable(GL_BLEND);
    glBlendFunc(ONE, ONE_MINUS_SRC_ALPHA);
    draw_sector_alpha();
    glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    rendercheckpoints();
    rendersprites();
    glDisable(CULL_FACE);
    glDepthMask(0);
    glEnable(GL_BLEND);
    glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    bind_image(sr_data_materiallist.flagged_material);
    glBlendFunc(ONE, ONE_MINUS_SRC_ALPHA);
    draw_windows();
    use_program(0);
    glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    glEnable(CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    rendermuzzleflashes();
    if (cl_data_selected_item != -1) {
        const Item *item = &sr_data_item[cl_data_selected_item];
        glDisable(GL_TEXTURE_2D);
        rendermodelwire(sr_data_modellist.item[item->type], &item->pos, item->rot, (const unsigned *)white, 1.125f);
        glEnable(GL_TEXTURE_2D);
    }
    stoptimer(sr_data_timerlist.renderalpha);
}

/* Draw the shapes of the world debug view. */
static void draw_world_debug(int world_view)
{
    int i;
    glDisable(GL_TEXTURE_2D);
    glDepthMask(1);
    polygon_mode(MODE_LINE);
    glDisable(GL_TEXTURE_2D);
    if (world_view)
        draw_debug_boxes();
    polygon_mode(MODE_FILL);
    if (world_view)
        draw_debug_items();
    polygon_mode(MODE_FILL);
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.24f, 0.25f, 0.25f);
    if (world_view)
        draw_debug_tracks();
    glBegin(LINES);
    glColor3f(1.0f, 1.0f, 0.0f);
    if (world_view)
        draw_debug_lines();
    glEnd();
    if (world_view)
        for (i = 0; i < sr_data_debug.sphere_count; ++i) {
            glColor3fv(&sr_data_debug.spheres[i].color.x);
            rendersphere(&sr_data_debug.spheres[i].pos, sr_data_debug.spheres[i].radius, 1);
        }
    if (world_view)
        draw_debug_wheels();
    glEnable(GL_BLEND);
    glColor3f(1.0f, 0.5f, 0.0f);
    glBegin(LINES);
    if (world_view)
        for (i = 0; i < sr_data_numofbullets; ++i) {
            glColor4f(1.0f, 0.5f, 0.0f, 0.5f);
            glVertex3fv(&sr_data_bullet[i].pos_prev.x);
            glColor4f(1.0f, 0.5f, 0.0f, 1.0f);
            glVertex3fv(&sr_data_bullet[i].pos.x);
        }
    glEnd();
}

/* Draw the color pass. The polygon debug view stops after the opaque geometry. */
static void draw_color(const ClientRenderOptions *options)
{
    frame_view();
    if (options->framebuffer)
        cl_glFramebufferTexture2D(FRAMEBUFFER, COLOR_ATTACHMENT, GL_TEXTURE_2D,
            cl_data_texture[sr_data_texturelist.frame_color].gl_name, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(CLEAR_COLOR);
    setup_light();
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
    if (sr_data_debug.display)
        polygon_mode(MODE_LINE);
    glEnable(CULL_FACE);
    glFrontFace(FRONT_CW);
    if (!sr_data_debug.display)
        glDepthMask(0);
    draw_opaque();
    if (sr_data_debug.display) {
        polygon_mode(MODE_FILL);
        return;
    }
    draw_alpha();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(0);
    glBegin(QUADS);
    glEnd();
    if (sr_data_game.gametype == GAMETYPE_RACE)
        draw_race_checkpoints();
    glEnable(GL_TEXTURE_2D);
    draw_elevator_panels();
    draw_world_debug(sr_data_debug.world_view != 0);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(0);
    if (!cl_data_audio_input_blocked && cl_data_keyboard[KEY_NAMES])
        draw_name_tags();
    glDepthMask(1);
    if (options->framebuffer)
        bind_framebuffer(0);
}

/* Draw one frame: depth and shadows first, then the color pass. The shadow debug key shows only the shadow volumes. */
void __cdecl renderframe(void)
{
    const ClientRenderOptions *options = (const ClientRenderOptions *)&sr_data_option;
    reset_timers();
    setup_view();
    clearsectorlist(1);
    buildframelist();
    setupframelandscapelod();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(DEPTH_LEQUAL);
    glDepthMask(1);
    if (options->framebuffer)
        bind_framebuffer(cl_data_framebuffers[cl_data_framebuffer_index].framebuffer);
    frame_view();
    if (!sr_data_debug.display)
        renderframedepth();
    else {
        glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
        glStencilMask(UINT32_MAX);
        glClearStencil(0);
        glClear(CLEAR_COLOR_DEPTH_STENCIL);
        glStencilMask(0);
    }
    if (sr_data_debugit && !cl_data_audio_input_blocked && cl_data_keyboard[KEY_SHADOWS]) {
        if (options->framebuffer)
            bind_framebuffer(0);
        return;
    }
    draw_color(options);
}
