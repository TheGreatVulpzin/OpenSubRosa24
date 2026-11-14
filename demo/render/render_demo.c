/* Subrosa Render Pipeline Demo: a part of the Sub Rosa city drawn with the passes of the client frame.
 *
 * The passes follow renderframe (src/client/render/frame.c) and renderframedepth
 * (src/client/render/depth.c): depth, shadow volumes, stencil count, shadow mask, ambient
 * occlusion, blur, opaque geometry, sky and sun, transparent surfaces.
 *
 * The scene comes from city.scene (demo/render/export_scene.py writes it from a client recording).
 * The texts of the side panel come from passes.txt. Both files sit next to the program.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

typedef char GLchar;
typedef GLuint(APIENTRY *CreateShaderFn)(GLenum);
typedef void(APIENTRY *ShaderSourceFn)(GLuint, GLsizei, const GLchar **, const GLint *);
typedef void(APIENTRY *CompileShaderFn)(GLuint);
typedef void(APIENTRY *GetShaderivFn)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *GetInfoLogFn)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint(APIENTRY *CreateProgramFn)(void);
typedef void(APIENTRY *AttachShaderFn)(GLuint, GLuint);
typedef void(APIENTRY *LinkProgramFn)(GLuint);
typedef void(APIENTRY *UseProgramFn)(GLuint);
typedef GLint(APIENTRY *GetUniformLocationFn)(GLuint, const GLchar *);
typedef void(APIENTRY *Uniform1iFn)(GLint, GLint);
typedef void(APIENTRY *Uniform2fFn)(GLint, GLfloat, GLfloat);
typedef void(APIENTRY *Uniform3fFn)(GLint, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY *ActiveTextureFn)(GLenum);

static CreateShaderFn glCreateShader;
static ShaderSourceFn glShaderSource;
static CompileShaderFn glCompileShader;
static GetShaderivFn glGetShaderiv;
static GetInfoLogFn glGetShaderInfoLog;
static CreateProgramFn glCreateProgram;
static AttachShaderFn glAttachShader;
static LinkProgramFn glLinkProgram;
static GetShaderivFn glGetProgramiv;
static GetInfoLogFn glGetProgramInfoLog;
static UseProgramFn glUseProgram;
static GetUniformLocationFn glGetUniformLocation;
static Uniform1iFn glUniform1i;
static Uniform2fFn glUniform2f;
static Uniform3fFn glUniform3f;
static ActiveTextureFn glActiveTexture;

typedef struct { float x, y, z; } vec3;

enum {
    NUM_PASSES = 9,
    PANEL_W = 400,
    FONT_BASE = 1000,
    FONT_W = 8,
    LINE_H = 17,
    NUM_CHUNKS = 48,
    MAX_WINDOWS = 512,
    MAX_TEXT = 8192,
    GRID = 256,
    GRID_Y = 64
};

/* Passes, in frame order. */
enum { P_DEPTH, P_VOLUMES, P_STENCIL, P_MASK, P_OCCLUSION, P_BLUR, P_OPAQUE, P_SKY, P_ALPHA };

typedef struct { vec3 low, high; } Box;
typedef struct { vec3 corners[4]; vec3 normal; float distance; int facing; } Window;
typedef struct { char title[64]; char calls[256]; char body[MAX_TEXT]; } PassText;

static HWND g_window;
static HDC g_dc;
static int g_width = 1240, g_height = 620;
static int g_view_w, g_view_h;
static int g_pass = P_ALPHA;
static int g_playing;
static int g_thumbs = 1;
static float g_progress = 1.0f;
static float g_yaw = 0.9f, g_pitch = 0.55f, g_distance = 300.0f;
static vec3 g_target = {0.0f, 30.0f, 0.0f};
static float g_sun_tilt = 0.85f, g_sun_heading = 2.3f;
static int g_drag, g_drag_x, g_drag_y;
static vec3 g_camera, g_light;
static PassText g_text[NUM_PASSES];

static Box *g_casters;
static int g_caster_count;
static Window g_windows[MAX_WINDOWS];
static int g_window_count;
static GLuint g_chunk_list[NUM_CHUNKS];
static GLuint g_volume_list;
static float g_volume_tilt = -1.0f, g_volume_heading;

static GLuint g_depth_tex, g_part_depth_tex, g_map_tex, g_random_tex, g_view_tex;
static GLuint g_stage_tex[NUM_PASSES];
static GLuint g_material_tex[64], g_white_tex;
static GLuint g_prog_depth_view, g_prog_occlusion, g_prog_blur, g_prog_lit, g_prog_sky;

static vec3 v3(float x, float y, float z) { vec3 v; v.x = x; v.y = y; v.z = z; return v; }
static vec3 add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static vec3 sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static vec3 scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static vec3 cross(vec3 a, vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static vec3 normalize(vec3 a) { float l = sqrtf(dot(a, a)); return l > 0 ? scale(a, 1.0f / l) : a; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* ---- OpenGL setup ---- */

static void *gl_proc(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if (!p) {
        MessageBoxA(NULL, name, "OpenGL 2.0 function missing", MB_OK);
        ExitProcess(1);
    }
    return p;
}

static void load_gl(void)
{
    glCreateShader = (CreateShaderFn)gl_proc("glCreateShader");
    glShaderSource = (ShaderSourceFn)gl_proc("glShaderSource");
    glCompileShader = (CompileShaderFn)gl_proc("glCompileShader");
    glGetShaderiv = (GetShaderivFn)gl_proc("glGetShaderiv");
    glGetShaderInfoLog = (GetInfoLogFn)gl_proc("glGetShaderInfoLog");
    glCreateProgram = (CreateProgramFn)gl_proc("glCreateProgram");
    glAttachShader = (AttachShaderFn)gl_proc("glAttachShader");
    glLinkProgram = (LinkProgramFn)gl_proc("glLinkProgram");
    glGetProgramiv = (GetShaderivFn)gl_proc("glGetProgramiv");
    glGetProgramInfoLog = (GetInfoLogFn)gl_proc("glGetProgramInfoLog");
    glUseProgram = (UseProgramFn)gl_proc("glUseProgram");
    glGetUniformLocation = (GetUniformLocationFn)gl_proc("glGetUniformLocation");
    glUniform1i = (Uniform1iFn)gl_proc("glUniform1i");
    glUniform2f = (Uniform2fFn)gl_proc("glUniform2f");
    glUniform3f = (Uniform3fFn)gl_proc("glUniform3f");
    glActiveTexture = (ActiveTextureFn)gl_proc("glActiveTexture");
}

static GLuint compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof log, NULL, log);
        MessageBoxA(NULL, log, "Shader error", MB_OK);
        ExitProcess(1);
    }
    return shader;
}

static GLuint program(const char *vertex, const char *fragment)
{
    GLuint p = glCreateProgram();
    GLint ok = 0;
    glAttachShader(p, compile(GL_VERTEX_SHADER, vertex));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fragment));
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        MessageBoxA(NULL, log, "Link error", MB_OK);
        ExitProcess(1);
    }
    return p;
}

static const char *const VS_QUAD =
    "void main() { gl_TexCoord[0] = gl_MultiTexCoord0; gl_Position = gl_Vertex; }";

static const char *const FS_DEPTH_VIEW =
    "uniform sampler2D depth;\n"
    "void main() {\n"
    "  float d = texture2D(depth, gl_TexCoord[0].xy).r;\n"
    "  float n = 1.0, f = 4000.0;\n"
    "  float z = n * f / (f - d * (f - n));\n"
    "  float g = d >= 1.0 ? 1.0 : clamp(log(z / 20.0) / log(60.0), 0.0, 0.92);\n"
    "  gl_FragColor = vec4(g, g, g, 1.0);\n"
    "}";

static const char *const FS_OCCLUSION =
    "uniform sampler2D depth;\n"
    "uniform sampler2D noise;\n"
    "uniform vec2 size;\n"
    "float linear(float d) { float n = 1.0, f = 4000.0; return n * f / (f - d * (f - n)); }\n"
    "void main() {\n"
    "  vec2 uv = gl_TexCoord[0].xy;\n"
    "  float z = linear(texture2D(depth, uv).r);\n"
    "  vec2 r = texture2D(noise, uv * size / 4.0).xy;\n"
    "  float radius = clamp(900.0 / z, 2.0, 36.0);\n"
    "  float hidden = 0.0;\n"
    "  for (int i = 0; i < 8; i++) {\n"
    "    float a = float(i) * 0.785398 + r.x * 6.28318;\n"
    "    float s = radius * (0.3 + 0.7 * fract(float(i) * 0.618 + r.y));\n"
    "    vec2 o = vec2(cos(a), sin(a)) * s / size;\n"
    "    float diff = z - linear(texture2D(depth, uv + o).r);\n"
    "    if (diff > 0.4 && diff < 16.0) hidden += 1.0;\n"
    "  }\n"
    "  gl_FragColor = vec4(0.0, 1.0 - hidden / 9.0, 0.0, 1.0);\n"
    "}";

static const char *const FS_BLUR =
    "uniform sampler2D map;\n"
    "uniform vec2 step;\n"
    "void main() {\n"
    "  vec2 uv = gl_TexCoord[0].xy;\n"
    "  vec4 sum = texture2D(map, uv) * 0.3;\n"
    "  sum += (texture2D(map, uv + step) + texture2D(map, uv - step)) * 0.2;\n"
    "  sum += (texture2D(map, uv + step * 2.0) + texture2D(map, uv - step * 2.0)) * 0.15;\n"
    "  gl_FragColor = vec4(sum.rg, 0.0, 1.0);\n"
    "}";

static const char *const VS_LIT =
    "varying vec3 normal;\n"
    "void main() {\n"
    "  normal = gl_Normal; gl_FrontColor = gl_Color; gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "  gl_Position = ftransform();\n"
    "}";

static const char *const FS_LIT =
    "uniform sampler2D base;\n"
    "uniform sampler2D map;\n"
    "uniform vec2 size;\n"
    "uniform vec3 light;\n"
    "varying vec3 normal;\n"
    "void main() {\n"
    "  vec2 m = texture2D(map, gl_FragCoord.xy / size).rg;\n"
    "  float sun = max(dot(normalize(normal), light), 0.0) * m.r;\n"
    "  vec3 albedo = gl_Color.rgb * texture2D(base, gl_TexCoord[0].xy).rgb;\n"
    "  vec3 c = albedo * (vec3(0.40, 0.44, 0.52) * (0.35 + 0.65 * m.g) + vec3(1.0, 0.93, 0.80) * sun * 0.95);\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}";

static const char *const FS_SKY =
    "uniform vec3 light;\n"
    "void main() {\n"
    "  float h = gl_TexCoord[0].y;\n"
    "  vec3 top = vec3(0.20, 0.38, 0.72), low = vec3(0.74, 0.82, 0.90);\n"
    "  vec3 c = mix(low, top, clamp(h, 0.0, 1.0));\n"
    "  c *= 0.55 + 0.45 * clamp(light.y * 2.0, 0.0, 1.0);\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}";

static GLuint make_texture(GLenum format, GLenum data_format, GLenum type)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, format, g_view_w, g_view_h, 0, data_format, type, NULL);
    return t;
}

static void make_textures(void)
{
    int i;
    unsigned char noise[4 * 4 * 4];
    g_view_w = g_width - PANEL_W;
    g_view_h = g_height;
    if (g_view_w < 64)
        g_view_w = 64;
    if (g_depth_tex) {
        glDeleteTextures(1, &g_depth_tex);
        glDeleteTextures(1, &g_part_depth_tex);
        glDeleteTextures(1, &g_map_tex);
        glDeleteTextures(1, &g_view_tex);
        glDeleteTextures(NUM_PASSES, g_stage_tex);
    }
    g_depth_tex = make_texture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
    g_part_depth_tex = make_texture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
    g_map_tex = make_texture(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    g_view_tex = make_texture(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    for (i = 0; i < NUM_PASSES; ++i)
        g_stage_tex[i] = make_texture(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    if (!g_random_tex) {
        srand(7);
        for (i = 0; i < (int)sizeof noise; ++i)
            noise[i] = (unsigned char)(rand() & 255);
        glGenTextures(1, &g_random_tex);
        glBindTexture(GL_TEXTURE_2D, g_random_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise);
    }
}

/* ---- texts ---- */

static void exe_path(char *out, const char *name)
{
    char *slash;
    GetModuleFileNameA(NULL, out, MAX_PATH);
    slash = strrchr(out, '\\');
    if (slash)
        strcpy(slash + 1, name);
}

/* Read passes.txt: "## " starts a pass, "calls: " gives its calls, "#" lines are comments. */
static void load_texts(void)
{
    char path[MAX_PATH], line[2048];
    FILE *f;
    int pass = -1;
    exe_path(path, "passes.txt");
    f = fopen(path, "r");
    if (!f) {
        strcpy(g_text[0].title, "passes.txt not found");
        return;
    }
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (strncmp(line, "## ", 3) == 0) {
            if (++pass >= NUM_PASSES)
                break;
            strncpy(g_text[pass].title, line + 3, sizeof g_text[pass].title - 1);
            continue;
        }
        if (line[0] == '#' || pass < 0)
            continue;
        if (strncmp(line, "calls: ", 7) == 0) {
            strncpy(g_text[pass].calls, line + 7, sizeof g_text[pass].calls - 1);
            continue;
        }
        if (strlen(g_text[pass].body) + n + 2 < MAX_TEXT) {
            strcat(g_text[pass].body, n ? line : "\n");
            strcat(g_text[pass].body, n ? " " : "");
        }
    }
    fclose(f);
}

/* ---- scene ---- */

static const float FACE_NORMALS[6][3] = {{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}};
/* Corners of each face in the order y-, z+, x-, z-, x+, y+, turned outward (unit cube). */
static const float FACE_CORNERS[6][4][3] = {
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}};

static const float PALETTE[8][3] = {{0.78f, 0.72f, 0.62f}, {0.66f, 0.42f, 0.34f}, {0.62f, 0.64f, 0.66f},
                                    {0.82f, 0.78f, 0.68f}, {0.52f, 0.56f, 0.62f}, {0.74f, 0.62f, 0.48f},
                                    {0.70f, 0.70f, 0.64f}, {0.58f, 0.50f, 0.44f}};

static void add_window(vec3 center, vec3 side, vec3 up, vec3 normal)
{
    Window *w;
    if (g_window_count >= MAX_WINDOWS)
        return;
    w = &g_windows[g_window_count++];
    w->corners[0] = sub(sub(center, side), up);
    w->corners[1] = sub(add(center, side), up);
    w->corners[2] = add(add(center, side), up);
    w->corners[3] = add(sub(center, side), up);
    w->normal = normal;
}

/* A vehicle: a body box and a cabin box along the rotation rows, with four windows on the cabin. */
static void add_vehicle(vec3 pos, const float *rot)
{
    vec3 side = v3(rot[0], rot[1], rot[2]), up = v3(rot[3], rot[4], rot[5]), forward = v3(rot[6], rot[7], rot[8]);
    vec3 cabin = add(pos, scale(up, 0.55f));
    add_window(add(cabin, scale(forward, 1.12f)), scale(side, 0.9f), scale(up, 0.3f), forward);
    add_window(sub(cabin, scale(forward, 1.12f)), scale(side, 0.9f), scale(up, 0.3f), scale(forward, -1));
    add_window(add(cabin, scale(side, 0.92f)), scale(forward, 1.0f), scale(up, 0.3f), side);
    add_window(sub(cabin, scale(side, 0.92f)), scale(forward, 1.0f), scale(up, 0.3f), scale(side, -1));
}

static void emit_oriented_box(vec3 pos, vec3 side, vec3 up, vec3 forward, float sx, float sy, float sz)
{
    int f, k;
    for (f = 0; f < 6; ++f) {
        vec3 n = add(add(scale(side, FACE_NORMALS[f][0]), scale(up, FACE_NORMALS[f][1])),
                     scale(forward, FACE_NORMALS[f][2]));
        glNormal3f(n.x, n.y, n.z);
        for (k = 0; k < 4; ++k) {
            const float *c = FACE_CORNERS[f][k];
            vec3 p = add(pos, add(add(scale(side, (c[0] * 2 - 1) * sx), scale(up, (c[1] * 2 - 1) * sy)),
                                  scale(forward, (c[2] * 2 - 1) * sz)));
            glVertex3f(p.x, p.y, p.z);
        }
    }
}

typedef struct { short x, y, z; unsigned char kind, faces, materials[6]; } Cell;

/* Load city.scene: cells into display lists by distance from the center, so the build animation
 * shows the city grow from the middle. Shadow casters are columns of cells, merged along x. */
static int load_scene(void)
{
    char path[MAX_PATH];
    FILE *f;
    char magic[4];
    int version, count, i, k, objects;
    float block;
    Cell *cells;
    unsigned char *occupied;
    short *top_y;
    int *order;
    float *dist;
    exe_path(path, "city.scene");
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "SRSC", 4) || fread(&version, 4, 1, f) != 1 || version != 3 ||
        fread(&block, 4, 1, f) != 1 || fread(&count, 4, 1, f) != 1) {
        fclose(f);
        return 0;
    }
    cells = (Cell *)malloc(sizeof(Cell) * count);
    for (i = 0; i < count; ++i) {
        Cell *c = &cells[i];
        fread(&c->x, 2, 1, f);
        fread(&c->y, 2, 1, f);
        fread(&c->z, 2, 1, f);
        fread(&c->kind, 1, 1, f);
        fread(&c->faces, 1, 1, f);
        fread(c->materials, 1, 6, f);
    }
    occupied = (unsigned char *)calloc(GRID * GRID * GRID_Y, 1);
    top_y = (short *)malloc(sizeof(short) * GRID * GRID);
    for (i = 0; i < GRID * GRID; ++i)
        top_y[i] = -1;
    for (i = 0; i < count; ++i) {
        int gx = cells[i].x + GRID / 2, gz = cells[i].z + GRID / 2, gy = cells[i].y;
        if (gx < 0 || gx >= GRID || gz < 0 || gz >= GRID || gy < 0 || gy >= GRID_Y)
            continue;
        occupied[(gy * GRID + gz) * GRID + gx] = 1;
        if (gy > top_y[gz * GRID + gx])
            top_y[gz * GRID + gx] = (short)gy;
    }
    order = (int *)malloc(sizeof(int) * count);
    dist = (float *)malloc(sizeof(float) * count);
    for (i = 0; i < count; ++i) {
        order[i] = i;
        dist[i] = sqrtf((float)(cells[i].x * cells[i].x + cells[i].z * cells[i].z));
    }
    {
        /* Sort by distance with a counting sort on whole cells. */
        int *buckets = (int *)calloc(GRID + 2, sizeof(int)), *sorted = (int *)malloc(sizeof(int) * count);
        for (i = 0; i < count; ++i)
            buckets[(int)dist[i] + 1]++;
        for (i = 1; i < GRID + 2; ++i)
            buckets[i] += buckets[i - 1];
        for (i = 0; i < count; ++i)
            sorted[buckets[(int)dist[i]]++] = i;
        memcpy(order, sorted, sizeof(int) * count);
        free(buckets);
        free(sorted);
    }
    {
        /* The vehicles and the material textures follow the cells in the file. */
        long cells_end = ftell(f);
        int m;
        if (fread(&objects, 4, 1, f) != 1)
            objects = 0;
        fseek(f, objects * (4 + 12 * 4), SEEK_CUR);
        for (m = 0; m < 64; ++m) {
            int w, h;
            unsigned char *pixels;
            if (fread(&w, 4, 1, f) != 1 || fread(&h, 4, 1, f) != 1 || w <= 0 || h <= 0 || w > 1024 || h > 1024)
                break;
            pixels = (unsigned char *)malloc((size_t)w * h * 4);
            fread(pixels, 4, (size_t)w * h, f);
            glGenTextures(1, &g_material_tex[m]);
            glBindTexture(GL_TEXTURE_2D, g_material_tex[m]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, 0x8191, GL_TRUE); /* GL_GENERATE_MIPMAP */
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            free(pixels);
        }
        fseek(f, cells_end, SEEK_SET);
    }
    for (k = 0; k < NUM_CHUNKS; ++k) {
        int first = (int)((long long)count * k / NUM_CHUNKS), last = (int)((long long)count * (k + 1) / NUM_CHUNKS);
        int material;
        g_chunk_list[k] = glGenLists(1);
        glNewList(g_chunk_list[k], GL_COMPILE);
        if (k == 0) {
            /* The ground around the scene. */
            glBindTexture(GL_TEXTURE_2D, g_white_tex);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glColor3f(0.30f, 0.33f, 0.27f);
            glTexCoord2f(0, 0);
            glVertex3f(-2000, 23.9f, -2000);
            glVertex3f(-2000, 23.9f, 2000);
            glVertex3f(2000, 23.9f, 2000);
            glVertex3f(2000, 23.9f, -2000);
            glEnd();
        }
        for (material = 0; material < 64; ++material) {
            int begun = 0;
            for (i = first; i < last; ++i) {
                const Cell *c = &cells[order[i]];
                int face;
                if (c->y < 5)
                    continue;
                for (face = 0; face < 6; ++face) {
                    int nx = c->x + GRID / 2 + (int)FACE_NORMALS[face][0], ny = c->y + (int)FACE_NORMALS[face][1],
                        nz = c->z + GRID / 2 + (int)FACE_NORMALS[face][2], corner;
                    if (c->materials[face] != material)
                        continue;
                    if (c->kind == 1 && !(c->faces & (1 << face)))
                        continue;
                    if (c->kind == 2 && nx >= 0 && nx < GRID && nz >= 0 && nz < GRID && ny >= 0 && ny < GRID_Y &&
                        occupied[(ny * GRID + nz) * GRID + nx])
                        continue;
                    if (!begun) {
                        glBindTexture(GL_TEXTURE_2D, g_material_tex[material] ? g_material_tex[material] : g_white_tex);
                        glBegin(GL_QUADS);
                        glColor3f(1, 1, 1);
                        begun = 1;
                    }
                    glNormal3fv(FACE_NORMALS[face]);
                    for (corner = 0; corner < 4; ++corner) {
                        const float *p = FACE_CORNERS[face][corner];
                        /* Each face shows the whole texture, upright on the walls. */
                        float u = face == 2 || face == 4 ? p[2] : p[0];
                        float v = face == 0 || face == 5 ? p[2] : 1.0f - p[1];
                        glTexCoord2f(u, v);
                        glVertex3f((c->x + p[0]) * block, (c->y + p[1]) * block, (c->z + p[2]) * block);
                    }
                }
            }
            if (begun)
                glEnd();
        }
        if (k == NUM_CHUNKS - 1) {
            /* Vehicles from the recording: a body and a cabin. */
            glBindTexture(GL_TEXTURE_2D, g_white_tex);
            glBegin(GL_QUADS);
            if (fread(&objects, 4, 1, f) != 1)
                objects = 0;
            for (i = 0; i < objects; ++i) {
                int type;
                float pos[3], rot[9];
                vec3 p, side, up, forward;
                if (fread(&type, 4, 1, f) != 1 || fread(pos, 4, 3, f) != 3 || fread(rot, 4, 9, f) != 9)
                    break;
                p = v3(pos[0], pos[1], pos[2]);
                side = v3(rot[0], rot[1], rot[2]);
                up = v3(rot[3], rot[4], rot[5]);
                forward = v3(rot[6], rot[7], rot[8]);
                glColor3f(0.62f, 0.14f, 0.12f);
                emit_oriented_box(p, side, up, forward, 1.05f, 0.45f, 2.3f);
                glColor3f(0.30f, 0.32f, 0.36f);
                emit_oriented_box(add(p, scale(up, 0.55f)), side, up, forward, 0.9f, 0.3f, 1.1f);
                add_vehicle(p, rot);
            }
            glEnd();
        }
        glEndList();
    }
    {
        /* Shadow casters: one box for each run of columns with the same top along x. */
        int x, z, cap = 4096;
        g_casters = (Box *)malloc(sizeof(Box) * cap);
        for (z = 0; z < GRID; ++z)
            for (x = 0; x < GRID;) {
                int top = top_y[z * GRID + x], end = x + 1;
                if (top < 7) {
                    ++x;
                    continue;
                }
                while (end < GRID && top_y[z * GRID + end] == top)
                    ++end;
                if (g_caster_count == cap) {
                    cap *= 2;
                    g_casters = (Box *)realloc(g_casters, sizeof(Box) * cap);
                }
                g_casters[g_caster_count].low = v3((x - GRID / 2) * block, 6 * block, (z - GRID / 2) * block);
                g_casters[g_caster_count].high = v3((end - GRID / 2) * block, (top + 1) * block, (z - GRID / 2 + 1) * block);
                ++g_caster_count;
                x = end;
            }
    }
    fclose(f);
    free(cells);
    free(occupied);
    free(top_y);
    free(order);
    free(dist);
    return 1;
}

/* ---- shadow volumes ---- */

static vec3 box_corner(const Box *box, int i)
{
    return v3(i & 1 ? box->high.x : box->low.x, i & 2 ? box->high.y : box->low.y, i & 4 ? box->high.z : box->low.z);
}

static const int BOX_FACES[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
static const float BOX_NORMALS[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

/* Extrude each silhouette edge away from the sun by a length. The quad turns outward from the
 * volume, so its front face adds to the stencil count and its back face takes away. The volume has
 * no caps: the count is right while the camera is outside every volume. */
static void emit_volumes(float length)
{
    vec3 away = scale(g_light, -length);
    int i, f, k, g, m;
    glBegin(GL_QUADS);
    for (i = 0; i < g_caster_count; ++i) {
        const Box *box = &g_casters[i];
        vec3 center = scale(add(box->low, box->high), 0.5f);
        int lit[6];
        for (f = 0; f < 6; ++f)
            lit[f] = BOX_NORMALS[f][0] * g_light.x + BOX_NORMALS[f][1] * g_light.y + BOX_NORMALS[f][2] * g_light.z > 0.0f;
        for (f = 0; f < 6; ++f) {
            if (!lit[f])
                continue;
            for (k = 0; k < 4; ++k) {
                int a = BOX_FACES[f][k], b = BOX_FACES[f][(k + 1) & 3], other = -1;
                vec3 pa, pb;
                for (g = 0; g < 6 && other < 0; ++g) {
                    int has_a = 0, has_b = 0;
                    if (g == f)
                        continue;
                    for (m = 0; m < 4; ++m) {
                        has_a |= BOX_FACES[g][m] == a;
                        has_b |= BOX_FACES[g][m] == b;
                    }
                    if (has_a && has_b)
                        other = g;
                }
                if (other < 0 || lit[other])
                    continue;
                pa = box_corner(box, a);
                pb = box_corner(box, b);
                if (dot(cross(sub(pb, pa), away), sub(scale(add(pa, pb), 0.5f), center)) < 0.0f) {
                    vec3 t = pa;
                    pa = pb;
                    pb = t;
                }
                glVertex3f(pa.x, pa.y, pa.z);
                glVertex3f(pb.x, pb.y, pb.z);
                glVertex3f(pb.x + away.x, pb.y + away.y, pb.z + away.z);
                glVertex3f(pa.x + away.x, pa.y + away.y, pa.z + away.z);
            }
        }
    }
    glEnd();
}

static void draw_volumes(float progress)
{
    const float length = 1200.0f;
    if (progress < 1.0f) {
        emit_volumes(length * progress * progress);
        return;
    }
    if (g_volume_tilt != g_sun_tilt || g_volume_heading != g_sun_heading) {
        if (!g_volume_list)
            g_volume_list = glGenLists(1);
        glNewList(g_volume_list, GL_COMPILE);
        emit_volumes(length);
        glEndList();
        g_volume_tilt = g_sun_tilt;
        g_volume_heading = g_sun_heading;
    }
    glCallList(g_volume_list);
}

static void draw_scene(float progress)
{
    int k, n = (int)ceilf(progress * NUM_CHUNKS);
    for (k = 0; k < n && k < NUM_CHUNKS; ++k)
        glCallList(g_chunk_list[k]);
}

/* ---- view ---- */

static void setup_view(void)
{
    const float near_plane = 1.0f, far_plane = 4000.0f, fov = 0.7f;
    float aspect = (float)g_view_h / (float)g_view_w;
    vec3 forward, right, up;
    float m[16];
    g_camera = add(g_target, v3(cosf(g_yaw) * cosf(g_pitch) * g_distance, sinf(g_pitch) * g_distance,
                                sinf(g_yaw) * cosf(g_pitch) * g_distance));
    forward = normalize(sub(g_target, g_camera));
    right = normalize(cross(forward, v3(0, 1, 0)));
    up = cross(right, forward);
    glViewport(0, 0, g_view_w, g_view_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-fov * near_plane, fov * near_plane, -fov * aspect * near_plane, fov * aspect * near_plane, near_plane,
              far_plane);
    glMatrixMode(GL_MODELVIEW);
    m[0] = right.x; m[4] = right.y; m[8] = right.z; m[12] = 0;
    m[1] = up.x; m[5] = up.y; m[9] = up.z; m[13] = 0;
    m[2] = -forward.x; m[6] = -forward.y; m[10] = -forward.z; m[14] = 0;
    m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
    glLoadMatrixf(m);
    glTranslatef(-g_camera.x, -g_camera.y, -g_camera.z);
}

static void screen_space(void)
{
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void full_quad(void)
{
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
}

static void copy_screen(GLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_view_w, g_view_h);
}

static void draw_sphere(vec3 center, float radius)
{
    int i, j;
    for (i = 0; i < 12; ++i) {
        float a0 = 3.14159f * i / 12 - 1.5708f, a1 = 3.14159f * (i + 1) / 12 - 1.5708f;
        glBegin(GL_QUAD_STRIP);
        for (j = 0; j <= 24; ++j) {
            float b = 6.28318f * j / 24;
            glVertex3f(center.x + radius * cosf(a0) * cosf(b), center.y + radius * sinf(a0),
                       center.z + radius * cosf(a0) * sinf(b));
            glVertex3f(center.x + radius * cosf(a1) * cosf(b), center.y + radius * sinf(a1),
                       center.z + radius * cosf(a1) * sinf(b));
        }
        glEnd();
    }
}

static int compare_windows(const void *a, const void *b)
{
    float da = ((const Window *)a)->distance, db = ((const Window *)b)->distance;
    return da < db ? 1 : da > db ? -1 : 0;
}

/* ---- the passes ---- */

static void begin_depth(void)
{
    setup_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glColorMask(1, 1, 1, 1);
    glClearColor(1, 1, 1, 0);
    glStencilMask(0xffffffff);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glStencilMask(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

static void pass_depth(float progress, GLuint target)
{
    begin_depth();
    glColorMask(0, 0, 0, 0);
    draw_scene(progress);
    glColorMask(1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, target);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_view_w, g_view_h);
}

static void pass_volumes(void)
{
    glDepthMask(GL_FALSE);
    glColorMask(0, 0, 0, 0);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xffffffff);
    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
    glCullFace(GL_BACK);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    draw_volumes(1.0f);
    glCullFace(GL_FRONT);
    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    draw_volumes(1.0f);
    glCullFace(GL_BACK);
    glColorMask(1, 1, 1, 1);
}

static void lit_grey(int on)
{
    if (on) {
        float position[4], ambient[4] = {0.38f, 0.38f, 0.40f, 1.0f};
        position[0] = g_light.x; position[1] = g_light.y; position[2] = g_light.z; position[3] = 0.0f;
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, position);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        glEnable(GL_COLOR_MATERIAL);
    } else {
        glDisable(GL_COLOR_MATERIAL);
        glDisable(GL_LIGHTING);
    }
}

/* The view of pass 2: the city with plain light and the volumes in see-through green. The
 * animation makes the volumes grow away from the sun. */
static void view_volumes(float progress)
{
    setup_view();
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.1f, 0.1f, 0.12f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    lit_grey(1);
    glEnable(GL_TEXTURE_2D);
    draw_scene(1.0f);
    glDisable(GL_TEXTURE_2D);
    lit_grey(0);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.1f, 1.0f, 0.2f, 0.06f);
    draw_volumes(progress);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    copy_screen(g_stage_tex[P_VOLUMES]);
}

static void view_stencil(void)
{
    static const float colors[4][3] = {{0, 0, 0}, {0.8f, 0.1f, 0.1f}, {0.95f, 0.85f, 0.1f}, {1, 1, 1}};
    int k;
    screen_space();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    for (k = 0; k < 4; ++k) {
        glStencilFunc(k < 3 ? GL_EQUAL : GL_LEQUAL, k, 0xff);
        glColor3fv(colors[k]);
        full_quad();
    }
    glDisable(GL_STENCIL_TEST);
    copy_screen(g_stage_tex[P_STENCIL]);
}

static void pass_shadow_mask(void)
{
    screen_space();
    glDisable(GL_DEPTH_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(1, 0, 0, 0);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_EQUAL, 0, 0xffffffff);
    glColor3f(1, 1, 1);
    full_quad();
    glDisable(GL_STENCIL_TEST);
    glColorMask(1, 1, 1, 1);
    copy_screen(g_stage_tex[P_MASK]);
}

static void pass_occlusion(void)
{
    glColorMask(0, 1, 0, 0);
    glUseProgram(g_prog_occlusion);
    glUniform1i(glGetUniformLocation(g_prog_occlusion, "depth"), 0);
    glUniform1i(glGetUniformLocation(g_prog_occlusion, "noise"), 1);
    glUniform2f(glGetUniformLocation(g_prog_occlusion, "size"), (float)g_view_w, (float)g_view_h);
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_random_tex);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_depth_tex);
    full_quad();
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glColorMask(1, 1, 1, 1);
    copy_screen(g_map_tex);
    copy_screen(g_stage_tex[P_OCCLUSION]);
}

static void pass_blur(void)
{
    int i;
    glUseProgram(g_prog_blur);
    glUniform1i(glGetUniformLocation(g_prog_blur, "map"), 0);
    for (i = 0; i < 2; ++i) {
        if (i == 0)
            glUniform2f(glGetUniformLocation(g_prog_blur, "step"), 1.125f / g_view_w, 0.0f);
        else
            glUniform2f(glGetUniformLocation(g_prog_blur, "step"), 0.0f, 1.125f / g_view_h);
        glBindTexture(GL_TEXTURE_2D, g_map_tex);
        full_quad();
        copy_screen(g_map_tex);
    }
    glUseProgram(0);
    copy_screen(g_stage_tex[P_BLUR]);
    glDisable(GL_TEXTURE_2D);
}

static void draw_opaque(float progress)
{
    glUseProgram(g_prog_lit);
    glUniform1i(glGetUniformLocation(g_prog_lit, "base"), 0);
    glUniform1i(glGetUniformLocation(g_prog_lit, "map"), 1);
    glUniform2f(glGetUniformLocation(g_prog_lit, "size"), (float)g_view_w, (float)g_view_h);
    glUniform3f(glGetUniformLocation(g_prog_lit, "light"), g_light.x, g_light.y, g_light.z);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_map_tex);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    draw_scene(progress);
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0);
}

static void pass_opaque(void)
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    setup_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    draw_opaque(1.0f);
    copy_screen(g_stage_tex[P_OPAQUE]);
}

static void draw_sky(void)
{
    screen_space();
    glDepthFunc(GL_EQUAL);
    glUseProgram(g_prog_sky);
    glUniform3f(glGetUniformLocation(g_prog_sky, "light"), g_light.x, g_light.y, g_light.z);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0.2f); glVertex3f(-1, -1, 1);
    glTexCoord2f(0, 0.2f); glVertex3f(1, -1, 1);
    glTexCoord2f(0, 1.3f); glVertex3f(1, 1, 1);
    glTexCoord2f(0, 1.3f); glVertex3f(-1, 1, 1);
    glEnd();
    glUseProgram(0);
    glDepthFunc(GL_LEQUAL);
}

static void draw_sun(float size)
{
    setup_view();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glColor3f(0.95f, 0.85f, 0.6f);
    glDisable(GL_CULL_FACE);
    draw_sphere(add(g_camera, scale(g_light, 3000.0f)), 80.0f * size);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

static void pass_sky_and_sun(void)
{
    draw_sky();
    draw_sun(1.0f);
    copy_screen(g_stage_tex[P_SKY]);
}

static void sort_windows(void)
{
    int i;
    for (i = 0; i < g_window_count; ++i) {
        Window *w = &g_windows[i];
        vec3 center = scale(add(add(w->corners[0], w->corners[1]), add(w->corners[2], w->corners[3])), 0.25f);
        w->distance = sqrtf(dot(sub(center, g_camera), sub(center, g_camera)));
        w->facing = dot(sub(g_camera, w->corners[0]), w->normal) >= 0.0f;
    }
    qsort(g_windows, g_window_count, sizeof g_windows[0], compare_windows);
}

/* Draw the first windows of the sorted list: the far ones come first. */
static void draw_windows(int count)
{
    int i, k;
    setup_view();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (i = 0; i < count && i < g_window_count; ++i) {
        const Window *w = &g_windows[i];
        float a = w->facing ? 0.25f : 0.1875f;
        glColor4f(0.17f * 0.5f, 0.19f * 0.5f, 0.24f * 0.5f, a);
        for (k = 0; k < 4; ++k)
            glVertex3f(w->corners[k].x, w->corners[k].y, w->corners[k].z);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

static void pass_alpha(void)
{
    sort_windows();
    draw_windows(g_window_count);
    glDepthMask(GL_TRUE);
    copy_screen(g_stage_tex[P_ALPHA]);
}

/* Run every pass with the full scene: the side thumbnails and the views read these results. */
static void run_frame(void)
{
    pass_depth(1.0f, g_depth_tex);
    pass_volumes();
    view_stencil();
    pass_shadow_mask();
    pass_occlusion();
    pass_blur();
    pass_opaque();
    pass_sky_and_sun();
    pass_alpha();
    view_volumes(1.0f);
}

/* ---- display ---- */

static void draw_stage(int stage, float x0, float y0, float x1, float y1, float u1)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stage == P_DEPTH ? g_depth_tex : g_stage_tex[stage]);
    if (stage == P_DEPTH) {
        glUseProgram(g_prog_depth_view);
        glUniform1i(glGetUniformLocation(g_prog_depth_view, "depth"), 0);
    }
    if (stage == P_OCCLUSION)
        glColor3f(0, 1, 0);
    else if (stage == P_MASK)
        glColor3f(1, 0, 0);
    else
        glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(u1, 0); glVertex2f(x0 + (x1 - x0) * u1, y0);
    glTexCoord2f(u1, 1); glVertex2f(x0 + (x1 - x0) * u1, y1);
    glTexCoord2f(0, 1); glVertex2f(x0, y1);
    glEnd();
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
}

/* The main view of the selected pass at the animation progress. */
static void draw_main_view(void)
{
    float t = clamp01(g_progress);
    switch (g_pass) {
    case P_DEPTH:
        /* The city grows from the middle into the depth buffer. */
        pass_depth(t, g_part_depth_tex);
        screen_space();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_part_depth_tex);
        glUseProgram(g_prog_depth_view);
        glUniform1i(glGetUniformLocation(g_prog_depth_view, "depth"), 0);
        glColor3f(1, 1, 1);
        full_quad();
        glUseProgram(0);
        glDisable(GL_TEXTURE_2D);
        break;
    case P_VOLUMES:
        view_volumes(t);
        break;
    case P_OPAQUE:
        /* The color fills in, piece by piece, over the black screen. */
        setup_view();
        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDepthMask(GL_TRUE);
        draw_opaque(t);
        break;
    case P_SKY:
        /* The sky fills in, then the sun grows. */
        screen_space();
        glDisable(GL_DEPTH_TEST);
        draw_stage(P_OPAQUE, -1, -1, 1, 1, 1);
        draw_stage(P_SKY, -1, -1, 1, 1, clamp01(t * 1.6f));
        break;
    case P_ALPHA:
        /* The windows appear one at a time from far to near. */
        screen_space();
        glDisable(GL_DEPTH_TEST);
        draw_stage(P_SKY, -1, -1, 1, 1, 1);
        if (t < 1.0f) {
            pass_depth(1.0f, g_part_depth_tex);
            screen_space();
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            draw_stage(P_SKY, -1, -1, 1, 1, 1);
            glEnable(GL_DEPTH_TEST);
            sort_windows();
            draw_windows((int)(t * g_window_count));
            glDepthMask(GL_TRUE);
        } else
            draw_stage(P_ALPHA, -1, -1, 1, 1, 1);
        break;
    default: {
        /* A screen pass sweeps across the image of the pass before it. */
        int before = g_pass == P_STENCIL ? P_DEPTH : g_pass - 1;
        screen_space();
        glDisable(GL_DEPTH_TEST);
        if (g_pass == P_STENCIL)
            glColor3f(0, 0, 0);
        draw_stage(before, -1, -1, 1, 1, 1);
        draw_stage(g_pass, -1, -1, 1, 1, t);
        if (t < 1.0f) {
            float x = -1 + 2 * t;
            glColor3f(1, 0.85f, 0.3f);
            glBegin(GL_LINES);
            glVertex2f(x, -1);
            glVertex2f(x, 1);
            glEnd();
        }
    }
    }
}

static void text(int x, int y, const char *s, int n)
{
    glRasterPos2f(-1 + 2.0f * x / g_width, 1 - 2.0f * y / g_height);
    glListBase(FONT_BASE - 32);
    glCallLists(n, GL_UNSIGNED_BYTE, s);
}

/* Draw a text block wrapped to a width in characters. A newline starts a new paragraph. */
static int wrapped(int x, int y, int columns, const char *s)
{
    while (*s) {
        int n = 0, last_space = -1;
        while (s[n] && s[n] != '\n' && n < columns) {
            if (s[n] == ' ')
                last_space = n;
            ++n;
        }
        if (s[n] && s[n] != '\n' && last_space > 0)
            n = last_space;
        text(x, y, s, n);
        y += LINE_H;
        s += n;
        if (*s == '\n') {
            y += LINE_H / 2;
            ++s;
        }
        while (*s == ' ')
            ++s;
    }
    return y;
}

static void rect(int x0, int y0, int x1, int y1)
{
    glBegin(GL_QUADS);
    glVertex2f(-1 + 2.0f * x0 / g_width, 1 - 2.0f * y0 / g_height);
    glVertex2f(-1 + 2.0f * x1 / g_width, 1 - 2.0f * y0 / g_height);
    glVertex2f(-1 + 2.0f * x1 / g_width, 1 - 2.0f * y1 / g_height);
    glVertex2f(-1 + 2.0f * x0 / g_width, 1 - 2.0f * y1 / g_height);
    glEnd();
}

/* The buffers that the later passes read, as small images along the bottom of the view. */
static void draw_thumbs(void)
{
    static const int thumbs[4] = {P_DEPTH, P_MASK, P_OCCLUSION, P_BLUR};
    static const char *const labels[4] = {"depth", "shadow mask (red)", "occlusion (green)", "blurred map"};
    int tw = (g_width - PANEL_W - 50) / 4, th, i;
    if (tw > 200)
        tw = 200;
    th = tw * g_view_h / g_view_w;
    glColor3f(0.08f, 0.09f, 0.11f);
    rect(g_width - 4 * (tw + 10) - 10, g_height - th - 40, g_width, g_height - 4);
    for (i = 0; i < 4; ++i) {
        int x0 = g_width - (4 - i) * (tw + 10), y0 = g_height - th - 30;
        draw_stage(thumbs[i], -1 + 2.0f * x0 / g_width, 1 - 2.0f * (y0 + th) / g_height,
                   -1 + 2.0f * (x0 + tw) / g_width, 1 - 2.0f * y0 / g_height, 1);
        glColor3f(0.75f, 0.77f, 0.80f);
        text(x0, y0 + th + 16, labels[i], (int)strlen(labels[i]));
    }
}

static void draw_panel(void)
{
    const int columns = (PANEL_W - 32) / FONT_W;
    int y = 28, i;
    char line[160];
    glViewport(0, 0, g_width, g_height);
    screen_space();
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.11f, 0.12f, 0.14f);
    rect(0, 0, PANEL_W, g_height);
    glColor3f(0.95f, 0.80f, 0.35f);
    text(16, y, "SUBROSA RENDER PIPELINE DEMO", 28);
    y += 26;
    for (i = 0; i < NUM_PASSES; ++i) {
        int n = snprintf(line, sizeof line, "%d  %s", i + 1, g_text[i].title);
        if (i == g_pass) {
            glColor3f(0.24f, 0.26f, 0.32f);
            rect(8, y - 13, PANEL_W - 8, y + 5);
            glColor3f(1, 1, 1);
        } else
            glColor3f(0.55f, 0.57f, 0.62f);
        text(16, y, line, n);
        y += LINE_H + 1;
    }
    y += 12;
    glColor3f(0.95f, 0.80f, 0.35f);
    text(16, y, g_text[g_pass].title, (int)strlen(g_text[g_pass].title));
    y += LINE_H + 4;
    if (g_text[g_pass].calls[0]) {
        glColor3f(0.55f, 0.75f, 0.95f);
        y = wrapped(16, y, columns, g_text[g_pass].calls) + 6;
    }
    glColor3f(0.88f, 0.88f, 0.86f);
    wrapped(16, y, columns, g_text[g_pass].body);
    glColor3f(0.45f, 0.47f, 0.52f);
    text(16, g_height - 30, g_playing ? "Space: pause    Left/Right: pass" : "Space: play     Left/Right: pass", 32);
    text(16, g_height - 12, "Q/E R/F: sun    T: buffers   Enter: skip", 40);
}

static void render(float dt)
{
    g_light = normalize(v3(sinf(g_sun_tilt) * -cosf(g_sun_heading), cosf(g_sun_tilt),
                           sinf(g_sun_tilt) * -sinf(g_sun_heading)));
    if (g_playing) {
        g_progress += dt / 4.0f;
        if (g_progress >= 1.6f) {
            g_progress = 0.0f;
            g_pass = (g_pass + 1) % NUM_PASSES;
        }
    }
    run_frame();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_main_view();
    /* The view is in the lower left corner of the window: keep it, then draw the panel and the view. */
    copy_screen(g_view_tex);
    glViewport(0, 0, g_width, g_height);
    glClearColor(0.11f, 0.12f, 0.14f, 1);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    screen_space();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_view_tex);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1 + 2.0f * PANEL_W / g_width, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1 + 2.0f * PANEL_W / g_width, 1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    draw_panel();
    if (g_thumbs)
        draw_thumbs();
    if (g_playing || g_progress < 1.0f) {
        /* The progress bar of the animation under the view. */
        glColor3f(0.95f, 0.80f, 0.35f);
        rect(PANEL_W, g_height - 4, PANEL_W + (int)((g_width - PANEL_W) * clamp01(g_progress)), g_height);
    }
    SwapBuffers(g_dc);
}

/* ---- window ---- */

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (LOWORD(lp) > PANEL_W + 64 && HIWORD(lp) > 64 && g_dc) {
            g_width = LOWORD(lp);
            g_height = HIWORD(lp);
            make_textures();
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
            PostQuitMessage(0);
        if (wp == VK_RIGHT || wp == VK_LEFT || (wp >= '1' && wp <= '9')) {
            if (wp == VK_RIGHT)
                g_pass = (g_pass + 1) % NUM_PASSES;
            else if (wp == VK_LEFT)
                g_pass = (g_pass + NUM_PASSES - 1) % NUM_PASSES;
            else
                g_pass = (int)(wp - '1');
            /* A manual change shows the pass build itself one time. */
            g_progress = 0.0f;
        }
        if (wp == VK_SPACE) {
            g_playing = !g_playing;
            if (g_playing && g_progress >= 1.0f)
                g_progress = 0.0f;
        }
        if (wp == VK_RETURN)
            g_progress = 1.0f;
        if (wp == 'T')
            g_thumbs = !g_thumbs;
        return 0;
    case WM_RBUTTONDOWN:
        g_drag = 1;
        g_drag_x = (short)LOWORD(lp);
        g_drag_y = (short)HIWORD(lp);
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONUP:
        g_drag = 0;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        if (g_drag) {
            int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
            g_yaw += (x - g_drag_x) * 0.006f;
            g_pitch += (y - g_drag_y) * 0.006f;
            if (g_pitch < 0.08f) g_pitch = 0.08f;
            if (g_pitch > 1.5f) g_pitch = 1.5f;
            g_drag_x = x;
            g_drag_y = y;
        }
        return 0;
    case WM_MOUSEWHEEL:
        g_distance *= (short)HIWORD(wp) > 0 ? 0.9f : 1.1f;
        if (g_distance < 20.0f) g_distance = 20.0f;
        if (g_distance > 900.0f) g_distance = 900.0f;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* A city of boxes for a start without city.scene. */
static void make_box_scene(void)
{
    int x, z;
    g_caster_count = 0;
    g_casters = (Box *)malloc(sizeof(Box) * 64);
    srand(1234);
    g_chunk_list[0] = glGenLists(1);
    glNewList(g_chunk_list[0], GL_COMPILE);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glColor3f(0.40f, 0.43f, 0.36f);
    glVertex3f(-2000, 24, -2000);
    glVertex3f(-2000, 24, 2000);
    glVertex3f(2000, 24, 2000);
    glVertex3f(2000, 24, -2000);
    for (x = -3; x <= 3; ++x)
        for (z = -3; z <= 3; ++z) {
            float cx = x * 48.0f, cz = z * 48.0f, half = 10.0f + (float)(rand() % 6), h = 30.0f + (float)(rand() % 60);
            if ((x + z) % 3 == 0 || g_caster_count >= 64)
                continue;
            g_casters[g_caster_count].low = v3(cx - half, 24, cz - half);
            g_casters[g_caster_count].high = v3(cx + half, 24 + h, cz + half);
            glColor3fv(PALETTE[g_caster_count & 7]);
            emit_oriented_box(v3(cx, 24 + h * 0.5f, cz), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), half, h * 0.5f, half);
            ++g_caster_count;
        }
    glEnd();
    glEndList();
    for (x = 1; x < NUM_CHUNKS; ++x) {
        g_chunk_list[x] = glGenLists(1);
        glNewList(g_chunk_list[x], GL_COMPILE);
        glEndList();
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    WNDCLASSA wc;
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC rc;
    HFONT font;
    MSG msg;
    RECT r = {0, 0, 0, 0};
    DWORD last = GetTickCount();
    (void)previous;
    (void)command;
    r.right = g_width;
    r.bottom = g_height;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "SubrosaRenderPipelineDemo";
    RegisterClassA(&wc);
    g_window = CreateWindowA(wc.lpszClassName, "Subrosa Render Pipeline Demo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, instance, NULL);
    g_dc = GetDC(g_window);
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    SetPixelFormat(g_dc, ChoosePixelFormat(g_dc, &pfd), &pfd);
    rc = wglCreateContext(g_dc);
    wglMakeCurrent(g_dc, rc);
    load_gl();
    font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, FIXED_PITCH,
                       "Consolas");
    SelectObject(g_dc, font);
    wglUseFontBitmapsA(g_dc, 32, 96, FONT_BASE);
    g_prog_depth_view = program(VS_QUAD, FS_DEPTH_VIEW);
    g_prog_occlusion = program(VS_QUAD, FS_OCCLUSION);
    g_prog_blur = program(VS_QUAD, FS_BLUR);
    g_prog_lit = program(VS_LIT, FS_LIT);
    g_prog_sky = program(VS_QUAD, FS_SKY);
    load_texts();
    {
        static const unsigned char white[4] = {255, 255, 255, 255};
        glGenTextures(1, &g_white_tex);
        glBindTexture(GL_TEXTURE_2D, g_white_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    }
    if (!load_scene())
        make_box_scene();
    make_textures();
    ShowWindow(g_window, show);
    for (;;) {
        DWORD now;
        float dt;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        now = GetTickCount();
        dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;
        if (GetKeyState('Q') & 0x8000) g_sun_heading -= dt;
        if (GetKeyState('E') & 0x8000) g_sun_heading += dt;
        if (GetKeyState('R') & 0x8000) g_sun_tilt -= dt * 0.5f;
        if (GetKeyState('F') & 0x8000) g_sun_tilt += dt * 0.5f;
        if (g_sun_tilt < 0.1f) g_sun_tilt = 0.1f;
        if (g_sun_tilt > 1.45f) g_sun_tilt = 1.45f;
        if (!g_playing && g_progress < 1.0f)
            g_progress = clamp01(g_progress + dt / 3.0f);
        render(dt);
    }
}
