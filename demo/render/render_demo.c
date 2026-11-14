/* Render demo: a small city scene drawn with the same passes as the Sub Rosa client frame.
 *
 * The frame runs these passes in order (see src/client/render/frame.c and depth.c):
 *   1. depth       the opaque geometry writes only depth
 *   2. volumes     the shadow volumes of the boxes change the stencil count
 *   3. stencil     the stencil count of each pixel (0 = lit)
 *   4. shadow mask the red channel is 1 where the stencil count is 0
 *   5. occlusion   the green channel gets ambient occlusion from the depth
 *   6. blur        two blur passes, across and down, step 1.125 / size
 *   7. opaque      the lit scene reads the red and green channels on screen
 *   8. sky and sun the sky behind, then the sun sphere with additive blend
 *   9. alpha       the car windows, sorted from far to near
 *
 * Keys: Left and Right or 1 to 9 select the pass. Q and E turn the sun, R and F tilt it.
 * Right mouse drag turns the camera, the wheel moves it nearer. T shows the buffer thumbnails.
 * Esc quits.
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

enum { NUM_PASSES = 9, MAX_BOXES = 64, MAX_WINDOWS = 8, FONT_BASE = 1000 };

static const char *const g_pass_name[NUM_PASSES] = {
    "1 depth", "2 shadow volumes", "3 stencil count", "4 shadow mask (red)", "5 occlusion (green)",
    "6 blur", "7 opaque", "8 sky and sun", "9 alpha windows"};

static const char *const g_pass_text[NUM_PASSES][4] = {
    {"renderframedepth: program 5, glClear(color, depth, stencil)",
     "render_sectorlist(1), render_sector_interior(1), renderlandscape(1), rendermodellist(1)",
     "Only depth matters here. The image shows depth: near is dark, far is light.", ""},
    {"glstateshadowtwoside, program 4: render_areasectorshadow, interior shadows, rendermodelshadow",
     "Each box extrudes its silhouette away from the sun to a far distance.",
     "Front faces add 1 to the stencil count, back faces take 1 away. Depth test on, depth write off.",
     "This view draws the volumes in green, as the shadow debug key of the game does."},
    {"The stencil count after the volumes. Black: count 0 (lit).",
     "Red: count 1, yellow: count 2, white: 3 or more.",
     "A pixel behind a front face and before the back face stays inside the shadow.", ""},
    {"glColorMask(1, 0, 0, 0), glStencilFunc(GL_EQUAL, 0), white quad over the screen",
     "The red channel becomes 1 where the stencil count is 0, so red means sunlight.",
     "glCopyTexSubImage2D copies the screen into the shadow map texture.", ""},
    {"glColorMask(0, 1, 0, 0), program 6 with the random vector texture on unit 1",
     "Each pixel compares its depth with 8 nearby samples. Near geometry darkens it.",
     "The green channel holds the ambient light that reaches the pixel.", ""},
    {"program 8, two passes: step (1.125 / width, 0), then (0, 1.125 / height)",
     "Each pass draws a quad with the shadow map and copies the result back to it.",
     "Soft edges for the shadow and the occlusion.", ""},
    {"render_sectorlist(0), renderlandscape(0), program 0 with the shadow map on unit 1",
     "Light = ambient * green + sun * max(N.L, 0) * red. The map is read at the screen position.",
     "rendermodellist(0), renderspritemodels(0), rendertrackmodel(0)", ""},
    {"renderframeatmosphere(0.625): the sky behind the scene",
     "glBlendFunc(GL_ONE, GL_ONE): rendersphere at camera + light * 2048, radius 32",
     "The sun color is the normalized diffuse light color.", ""},
    {"tiremark_render, render_sector_alpha, rendercheckpoints, rendersprites",
     "Car windows: the sort key is 16777216 - 256 * distance, so far windows draw first.",
     "Alpha 0.25 for a window that faces the camera, 0.1875 for the other side.",
     "glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA), color = half the ambient light."}};

typedef struct { vec3 low, high; float color[3]; } Box;
typedef struct { vec3 corners[4]; vec3 normal; float distance; int facing; } Window;

static HWND g_window;
static HDC g_dc;
static int g_width = 1100, g_height = 640;
static int g_pass = NUM_PASSES - 1;
static int g_thumbs = 1;
static float g_yaw = 0.6f, g_pitch = 0.5f, g_distance = 190.0f;
static float g_sun_tilt = 0.75f, g_sun_heading = 2.3f;
static int g_drag, g_drag_x, g_drag_y;
static Box g_boxes[MAX_BOXES];
static int g_box_count;
static Window g_windows[MAX_WINDOWS];
static int g_window_count;
static vec3 g_camera, g_light;
static GLuint g_depth_tex, g_map_tex, g_random_tex;
static GLuint g_stage_tex[NUM_PASSES];
static GLuint g_prog_depth_view, g_prog_occlusion, g_prog_blur, g_prog_lit, g_prog_sky;

static vec3 v3(float x, float y, float z) { vec3 v = {x, y, z}; return v; }
static vec3 add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static vec3 sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static vec3 scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static vec3 cross(vec3 a, vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static vec3 normalize(vec3 a) { float l = sqrtf(dot(a, a)); return l > 0 ? scale(a, 1.0f / l) : a; }

/* ---- OpenGL setup ---- */

static void *gl_proc(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if (!p) {
        MessageBoxA(NULL, name, "OpenGL function missing", MB_OK);
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
    "  float n = 1.0, f = 2000.0;\n"
    "  float z = n * f / (f - d * (f - n));\n"
    "  float g = clamp(log(z) / log(400.0), 0.0, 1.0);\n"
    "  gl_FragColor = vec4(g, g, g, 1.0);\n"
    "}";

static const char *const FS_OCCLUSION =
    "uniform sampler2D depth;\n"
    "uniform sampler2D noise;\n"
    "uniform vec2 size;\n"
    "float linear(float d) { float n = 1.0, f = 2000.0; return n * f / (f - d * (f - n)); }\n"
    "void main() {\n"
    "  vec2 uv = gl_TexCoord[0].xy;\n"
    "  float z = linear(texture2D(depth, uv).r);\n"
    "  vec2 r = texture2D(noise, uv * size / 4.0).xy * 2.0 - 1.0;\n"
    "  float radius = clamp(600.0 / z, 2.0, 40.0);\n"
    "  float hidden = 0.0;\n"
    "  for (int i = 0; i < 8; i++) {\n"
    "    float a = float(i) * 0.785398 + r.x * 3.14159;\n"
    "    float s = radius * (0.35 + 0.65 * fract(float(i) * 0.618 + r.y));\n"
    "    vec2 o = vec2(cos(a), sin(a)) * s / size;\n"
    "    float sz = linear(texture2D(depth, uv + o).r);\n"
    "    float diff = z - sz;\n"
    "    if (diff > 0.3 && diff < 12.0) hidden += 1.0;\n"
    "  }\n"
    "  gl_FragColor = vec4(0.0, 1.0 - hidden / 10.0, 0.0, 1.0);\n"
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
    "void main() { normal = gl_Normal; gl_FrontColor = gl_Color; gl_Position = ftransform(); }";

static const char *const FS_LIT =
    "uniform sampler2D map;\n"
    "uniform vec2 size;\n"
    "uniform vec3 light;\n"
    "varying vec3 normal;\n"
    "void main() {\n"
    "  vec2 m = texture2D(map, gl_FragCoord.xy / size).rg;\n"
    "  float sun = max(dot(normalize(normal), light), 0.0) * m.r;\n"
    "  vec3 c = gl_Color.rgb * (vec3(0.30, 0.34, 0.42) * (0.4 + 0.6 * m.g) + vec3(1.0, 0.92, 0.78) * sun * 0.85);\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}";

static const char *const FS_SKY =
    "uniform vec3 light;\n"
    "void main() {\n"
    "  float h = gl_TexCoord[0].y;\n"
    "  vec3 top = vec3(0.18, 0.36, 0.72), low = vec3(0.72, 0.82, 0.92);\n"
    "  vec3 c = mix(low, top, clamp(h, 0.0, 1.0));\n"
    "  c *= 0.5 + 0.5 * clamp(light.y * 2.0, 0.0, 1.0);\n"
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
    glTexImage2D(GL_TEXTURE_2D, 0, format, g_width, g_height, 0, data_format, type, NULL);
    return t;
}

static void make_textures(void)
{
    int i;
    unsigned char noise[4 * 4 * 4];
    if (g_depth_tex) {
        glDeleteTextures(1, &g_depth_tex);
        glDeleteTextures(1, &g_map_tex);
        glDeleteTextures(NUM_PASSES, g_stage_tex);
    }
    g_depth_tex = make_texture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
    g_map_tex = make_texture(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
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

/* ---- scene ---- */

static float frand(float low, float high) { return low + (high - low) * (float)rand() / (float)RAND_MAX; }

static void add_box(vec3 low, vec3 high, float r, float g, float b)
{
    Box *box = &g_boxes[g_box_count++];
    box->low = low;
    box->high = high;
    box->color[0] = r;
    box->color[1] = g;
    box->color[2] = b;
}

static void make_scene(void)
{
    int x, z;
    srand(1234);
    for (x = -3; x <= 3; ++x)
        for (z = -3; z <= 3; ++z) {
            float cx = x * 36.0f, cz = z * 36.0f, half = frand(8.0f, 13.0f), h = frand(10.0f, 55.0f);
            float shade = frand(0.6f, 0.85f);
            if ((x == 0 && z == 0) || (x == 1 && z == 0) || g_box_count >= MAX_BOXES - 2)
                continue;
            if ((x + z) % 3 == 0)
                continue;
            add_box(v3(cx - half, 0, cz - half), v3(cx + half, h, cz + half), shade, shade * 0.95f, shade * 0.9f);
        }
    /* The car: a body and a roof. Its windows are the alpha surfaces. */
    add_box(v3(14, 0.4f, -2.2f), v3(22, 1.6f, 2.2f), 0.7f, 0.15f, 0.12f);
    add_box(v3(16, 1.6f, -2.0f), v3(20, 2.6f, 2.0f), 0.7f, 0.15f, 0.12f);
    g_window_count = 0;
    {
        vec3 a = v3(16, 1.6f, -2.0f), b = v3(20, 2.6f, 2.0f);
        Window *w;
        w = &g_windows[g_window_count++]; /* front */
        w->corners[0] = v3(20.02f, a.y, a.z); w->corners[1] = v3(20.02f, a.y, b.z);
        w->corners[2] = v3(20.02f, b.y, b.z); w->corners[3] = v3(20.02f, b.y, a.z); w->normal = v3(1, 0, 0);
        w = &g_windows[g_window_count++]; /* back */
        w->corners[0] = v3(15.98f, a.y, b.z); w->corners[1] = v3(15.98f, a.y, a.z);
        w->corners[2] = v3(15.98f, b.y, a.z); w->corners[3] = v3(15.98f, b.y, b.z); w->normal = v3(-1, 0, 0);
        w = &g_windows[g_window_count++]; /* left */
        w->corners[0] = v3(a.x, a.y, -2.02f); w->corners[1] = v3(b.x, a.y, -2.02f);
        w->corners[2] = v3(b.x, b.y, -2.02f); w->corners[3] = v3(a.x, b.y, -2.02f); w->normal = v3(0, 0, -1);
        w = &g_windows[g_window_count++]; /* right */
        w->corners[0] = v3(b.x, a.y, 2.02f); w->corners[1] = v3(a.x, a.y, 2.02f);
        w->corners[2] = v3(a.x, b.y, 2.02f); w->corners[3] = v3(b.x, b.y, 2.02f); w->normal = v3(0, 0, 1);
    }
}

static vec3 box_corner(const Box *box, int i)
{
    return v3(i & 1 ? box->high.x : box->low.x, i & 2 ? box->high.y : box->low.y, i & 4 ? box->high.z : box->low.z);
}

/* The faces of a box: corner indices in order around the face, outward normal. */
static const int FACES[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
static const float NORMALS[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

static void draw_box(const Box *box, int with_color)
{
    int f, k;
    for (f = 0; f < 6; ++f) {
        glNormal3fv(NORMALS[f]);
        if (with_color)
            glColor3fv(box->color);
        for (k = 0; k < 4; ++k) {
            vec3 c = box_corner(box, FACES[f][k]);
            glVertex3f(c.x, c.y, c.z);
        }
    }
}

static void draw_scene(int with_color)
{
    int i;
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    if (with_color)
        glColor3f(0.42f, 0.45f, 0.40f);
    glVertex3f(-400, 0, -400);
    glVertex3f(-400, 0, 400);
    glVertex3f(400, 0, 400);
    glVertex3f(400, 0, -400);
    for (i = 0; i < g_box_count; ++i)
        draw_box(&g_boxes[i], with_color);
    glEnd();
}

/* Extrude each silhouette edge of each box away from the sun. The quad turns outward from the
 * volume, so its front face adds to the stencil count and its back face takes away. The volume has
 * no caps: the count is right while the camera is outside every volume. */
static void draw_volumes(void)
{
    const float far_distance = 600.0f;
    vec3 away = scale(g_light, -far_distance);
    int i, f, k, g, m;
    glBegin(GL_QUADS);
    for (i = 0; i < g_box_count; ++i) {
        const Box *box = &g_boxes[i];
        int lit[6];
        for (f = 0; f < 6; ++f)
            lit[f] = NORMALS[f][0] * g_light.x + NORMALS[f][1] * g_light.y + NORMALS[f][2] * g_light.z > 0.0f;
        for (f = 0; f < 6; ++f) {
            vec3 center = scale(add(box->low, box->high), 0.5f);
            if (!lit[f])
                continue;
            for (k = 0; k < 4; ++k) {
                int a = FACES[f][k], b = FACES[f][(k + 1) & 3], other = -1;
                vec3 pa, pb, quad_normal, outward;
                for (g = 0; g < 6 && other < 0; ++g) {
                    int has_a = 0, has_b = 0;
                    if (g == f)
                        continue;
                    for (m = 0; m < 4; ++m) {
                        has_a |= FACES[g][m] == a;
                        has_b |= FACES[g][m] == b;
                    }
                    if (has_a && has_b)
                        other = g;
                }
                if (other < 0 || lit[other])
                    continue;
                pa = box_corner(box, a);
                pb = box_corner(box, b);
                quad_normal = cross(sub(pb, pa), away);
                outward = sub(scale(add(pa, pb), 0.5f), center);
                if (dot(quad_normal, outward) >= 0.0f) {
                    glVertex3f(pa.x, pa.y, pa.z);
                    glVertex3f(pb.x, pb.y, pb.z);
                    glVertex3f(pb.x + away.x, pb.y + away.y, pb.z + away.z);
                    glVertex3f(pa.x + away.x, pa.y + away.y, pa.z + away.z);
                } else {
                    glVertex3f(pb.x, pb.y, pb.z);
                    glVertex3f(pa.x, pa.y, pa.z);
                    glVertex3f(pa.x + away.x, pa.y + away.y, pa.z + away.z);
                    glVertex3f(pb.x + away.x, pb.y + away.y, pb.z + away.z);
                }
            }
        }
    }
    glEnd();
}

/* ---- view ---- */

static void setup_view(void)
{
    const float near_plane = 1.0f, far_plane = 2000.0f, fov = 0.75f;
    float aspect = (float)g_height / (float)g_width;
    vec3 target = v3(10, 4, 0), forward, right, up;
    float m[16];
    g_camera = add(target, v3(cosf(g_yaw) * cosf(g_pitch) * g_distance, sinf(g_pitch) * g_distance,
                              sinf(g_yaw) * cosf(g_pitch) * g_distance));
    forward = normalize(sub(target, g_camera));
    right = normalize(cross(forward, v3(0, 1, 0)));
    up = cross(right, forward);
    glViewport(0, 0, g_width, g_height);
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
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_width, g_height);
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

static void pass_depth(void)
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
    glColorMask(0, 0, 0, 0);
    draw_scene(0);
    glColorMask(1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, g_depth_tex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_width, g_height);
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
    draw_volumes();
    glCullFace(GL_FRONT);
    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    draw_volumes();
    glCullFace(GL_BACK);
    glColorMask(1, 1, 1, 1);
}

/* The view of pass 2: the scene in grey with the volumes in see-through green. */
static void view_volumes(void)
{
    setup_view();
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.1f, 0.1f, 0.12f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    {
        float position[4] = {g_light.x, g_light.y, g_light.z, 0.0f};
        float ambient[4] = {0.35f, 0.35f, 0.35f, 1.0f};
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, position);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        glEnable(GL_COLOR_MATERIAL);
        glColor3f(0.6f, 0.6f, 0.6f);
        draw_scene(0);
        glDisable(GL_COLOR_MATERIAL);
        glDisable(GL_LIGHTING);
    }
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 1.0f, 0.0f, 0.125f);
    draw_volumes();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    copy_screen(g_stage_tex[1]);
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
    copy_screen(g_stage_tex[2]);
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
    copy_screen(g_stage_tex[3]);
}

static void pass_occlusion(void)
{
    glColorMask(0, 1, 0, 0);
    glUseProgram(g_prog_occlusion);
    glUniform1i(glGetUniformLocation(g_prog_occlusion, "depth"), 0);
    glUniform1i(glGetUniformLocation(g_prog_occlusion, "noise"), 1);
    glUniform2f(glGetUniformLocation(g_prog_occlusion, "size"), (float)g_width, (float)g_height);
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
    copy_screen(g_stage_tex[4]);
}

static void pass_blur(void)
{
    int i;
    glUseProgram(g_prog_blur);
    glUniform1i(glGetUniformLocation(g_prog_blur, "map"), 0);
    for (i = 0; i < 2; ++i) {
        if (i == 0)
            glUniform2f(glGetUniformLocation(g_prog_blur, "step"), 1.125f / g_width, 0.0f);
        else
            glUniform2f(glGetUniformLocation(g_prog_blur, "step"), 0.0f, 1.125f / g_height);
        glBindTexture(GL_TEXTURE_2D, g_map_tex);
        full_quad();
        copy_screen(g_map_tex);
    }
    glUseProgram(0);
    copy_screen(g_stage_tex[5]);
    glDisable(GL_TEXTURE_2D);
}

static void pass_opaque(void)
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    setup_view();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glUseProgram(g_prog_lit);
    glUniform1i(glGetUniformLocation(g_prog_lit, "map"), 0);
    glUniform2f(glGetUniformLocation(g_prog_lit, "size"), (float)g_width, (float)g_height);
    glUniform3f(glGetUniformLocation(g_prog_lit, "light"), g_light.x, g_light.y, g_light.z);
    glBindTexture(GL_TEXTURE_2D, g_map_tex);
    draw_scene(1);
    glUseProgram(0);
    copy_screen(g_stage_tex[6]);
}

static void pass_sky_and_sun(void)
{
    /* The sky fills each pixel with no geometry: the depth there is still 1. */
    screen_space();
    glDepthFunc(GL_EQUAL);
    glUseProgram(g_prog_sky);
    glUniform3f(glGetUniformLocation(g_prog_sky, "light"), g_light.x, g_light.y, g_light.z);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0.3f); glVertex3f(-1, -1, 1);
    glTexCoord2f(0, 0.3f); glVertex3f(1, -1, 1);
    glTexCoord2f(0, 1.2f); glVertex3f(1, 1, 1);
    glTexCoord2f(0, 1.2f); glVertex3f(-1, 1, 1);
    glEnd();
    glUseProgram(0);
    glDepthFunc(GL_LEQUAL);
    setup_view();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glColor3f(0.95f, 0.85f, 0.6f);
    glDisable(GL_CULL_FACE);
    draw_sphere(add(g_camera, scale(g_light, 1500.0f)), 40.0f);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    copy_screen(g_stage_tex[7]);
}

static void pass_alpha(void)
{
    int i, k;
    for (i = 0; i < g_window_count; ++i) {
        Window *w = &g_windows[i];
        vec3 center = scale(add(add(w->corners[0], w->corners[1]), add(w->corners[2], w->corners[3])), 0.25f);
        w->distance = sqrtf(dot(sub(center, g_camera), sub(center, g_camera)));
        w->facing = dot(sub(g_camera, w->corners[0]), w->normal) >= 0.0f;
    }
    qsort(g_windows, g_window_count, sizeof g_windows[0], compare_windows);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (i = 0; i < g_window_count; ++i) {
        const Window *w = &g_windows[i];
        glColor4f(0.15f, 0.17f, 0.21f, w->facing ? 0.25f : 0.1875f);
        for (k = 0; k < 4; ++k)
            glVertex3f(w->corners[k].x, w->corners[k].y, w->corners[k].z);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    copy_screen(g_stage_tex[8]);
}

/* ---- display ---- */

/* Draw a stage texture. Pass 0 shows depth as grey, pass 4 keeps only the green channel. */
static void draw_stage(int stage, float x0, float y0, float x1, float y1)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, stage == 0 ? g_depth_tex : g_stage_tex[stage]);
    if (stage == 0) {
        glUseProgram(g_prog_depth_view);
        glUniform1i(glGetUniformLocation(g_prog_depth_view, "depth"), 0);
    }
    if (stage == 4)
        glColor3f(0, 1, 0);
    else
        glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(1, 0); glVertex2f(x1, y0);
    glTexCoord2f(1, 1); glVertex2f(x1, y1);
    glTexCoord2f(0, 1); glVertex2f(x0, y1);
    glEnd();
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
}

static void text(float x, float y, const char *s)
{
    glRasterPos2f(x, y);
    glListBase(FONT_BASE - 32);
    glCallLists((GLsizei)strlen(s), GL_UNSIGNED_BYTE, s);
}

static void draw_overlay(void)
{
    float px = 2.0f / g_width, py = 2.0f / g_height;
    char line[160];
    int i;
    screen_space();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(-1, 1);
    glVertex2f(-1 + 760 * px, 1);
    glVertex2f(-1 + 760 * px, 1 - 120 * py);
    glVertex2f(-1, 1 - 120 * py);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(1, 0.85f, 0.3f);
    snprintf(line, sizeof line, "Pass %s   (Left/Right or 1-9)", g_pass_name[g_pass]);
    text(-1 + 10 * px, 1 - 22 * py, line);
    glColor3f(1, 1, 1);
    for (i = 0; i < 4; ++i)
        if (g_pass_text[g_pass][i][0])
            text(-1 + 10 * px, 1 - (44 + i * 18) * py, g_pass_text[g_pass][i]);
    glColor3f(0.7f, 0.7f, 0.7f);
    text(-1 + 10 * px, -1 + 10 * py, "Q/E turn the sun, R/F tilt it, right drag turns the camera, wheel zooms, T thumbnails");
    if (g_thumbs) {
        static const int shown[5] = {0, 3, 4, 5, 8};
        static const char *const labels[5] = {"depth", "shadow mask", "occlusion", "blurred map", "final"};
        float th = (g_height - 10.0f) / 5.0f - 24.0f, tw = th * g_width / g_height;
        float w = tw * px, h = th * py;
        for (i = 0; i < 5; ++i) {
            float x1 = 1 - 10 * px, x0 = x1 - w, y1 = 1 - (10 + i * (th + 24)) * py, y0 = y1 - h;
            draw_stage(shown[i], x0, y0, x1, y1);
            glColor3f(1, 1, 0.6f);
            text(x0, y0 - 16 * py, labels[i]);
        }
    }
}

static void render(void)
{
    g_light = normalize(v3(sinf(g_sun_tilt) * -cosf(g_sun_heading), cosf(g_sun_tilt), sinf(g_sun_tilt) * -sinf(g_sun_heading)));
    pass_depth();
    pass_volumes();
    view_stencil();
    pass_shadow_mask();
    pass_occlusion();
    pass_blur();
    /* The screen quads did not write depth, so the color pass reuses the depth of pass 1. */
    pass_opaque();
    pass_sky_and_sun();
    pass_alpha();
    view_volumes();
    screen_space();
    glDisable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_stage(g_pass, -1, -1, 1, 1);
    draw_overlay();
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
        if (LOWORD(lp) > 0 && HIWORD(lp) > 0 && g_dc) {
            g_width = LOWORD(lp);
            g_height = HIWORD(lp);
            make_textures();
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
            PostQuitMessage(0);
        if (wp == VK_RIGHT)
            g_pass = (g_pass + 1) % NUM_PASSES;
        if (wp == VK_LEFT)
            g_pass = (g_pass + NUM_PASSES - 1) % NUM_PASSES;
        if (wp >= '1' && wp <= '9')
            g_pass = (int)(wp - '1');
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
            if (g_pitch < 0.05f) g_pitch = 0.05f;
            if (g_pitch > 1.5f) g_pitch = 1.5f;
            g_drag_x = x;
            g_drag_y = y;
        }
        return 0;
    case WM_MOUSEWHEEL:
        g_distance *= (short)HIWORD(wp) > 0 ? 0.9f : 1.1f;
        if (g_distance < 15.0f) g_distance = 15.0f;
        if (g_distance > 600.0f) g_distance = 600.0f;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    WNDCLASSA wc;
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC rc;
    HFONT font;
    MSG msg;
    DWORD last = GetTickCount();
    (void)previous;
    (void)command;
    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "SubRosaRenderDemo";
    RegisterClassA(&wc);
    g_window = CreateWindowA(wc.lpszClassName, "Sub Rosa render demo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, g_width + 16, g_height + 39, NULL, NULL, instance, NULL);
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
    font = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, FIXED_PITCH,
                       "Consolas");
    SelectObject(g_dc, font);
    wglUseFontBitmapsA(g_dc, 32, 96, FONT_BASE);
    g_prog_depth_view = program(VS_QUAD, FS_DEPTH_VIEW);
    g_prog_occlusion = program(VS_QUAD, FS_OCCLUSION);
    g_prog_blur = program(VS_QUAD, FS_BLUR);
    g_prog_lit = program(VS_LIT, FS_LIT);
    g_prog_sky = program(VS_QUAD, FS_SKY);
    make_scene();
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
        if (GetKeyState('Q') & 0x8000) g_sun_heading -= dt;
        if (GetKeyState('E') & 0x8000) g_sun_heading += dt;
        if (GetKeyState('R') & 0x8000) g_sun_tilt -= dt * 0.5f;
        if (GetKeyState('F') & 0x8000) g_sun_tilt += dt * 0.5f;
        if (g_sun_tilt < 0.1f) g_sun_tilt = 0.1f;
        if (g_sun_tilt > 1.45f) g_sun_tilt = 1.45f;
        render();
    }
}
