/* The shader programs of the renderer.
 *
 * A program record holds the OpenGL objects of one program and the places of its uniform
 * variables. The info logs of the compile and link steps go to one text log.
 */
#include "render/programs.h"

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "gl_slots.h"
#include "render/source.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_programs cl_data_programs
#define g_glext cl_data_glext
#define g_shader_log cl_data_shader_log
#define g_info_log cl_data_info_log
#define g_width cl_data_render_width
#define g_height cl_data_render_height
#define g_open_file cl_data_open_file

enum {
    GLEXT_SHADER_OBJECTS = 1,
    PROGRAM_SECTOR = 0,
    PROGRAM_WINDOW = 1,
    PROGRAM_REFLECT = 2,
    PROGRAM_MODEL = 3,
    PROGRAM_SHADOW = 4,
    PROGRAM_DEPTH = 5,
    PROGRAM_OCCLUSION = 6,
    PROGRAM_ATMOSPHERE = 7,
    PROGRAM_BLUR = 8,
    PROGRAM_LANDSCAPE = 9,
    GLEXT_VERTEX_SHADER = 2,
    GLEXT_FRAGMENT_SHADER = 3,
    INFO_LOG_SIZE = 0x10000
};

/* The functions of the shader extension: the client holds a pointer to each one. */
/* The constants of the atmosphere: four floats and three floats. */

typedef void(__stdcall *ShaderSourceFn)(uint32_t shader, int count, const char **source, const int *length);
typedef void(__stdcall *ShaderFn)(uint32_t shader);
typedef void(__stdcall *AttachObjectFn)(uint32_t program, uint32_t shader);
typedef void(__stdcall *GetInfoLogFn)(uint32_t object, int size, int *length, char *log);
typedef void(__stdcall *UseProgramFn)(uint32_t program);
typedef int(__stdcall *UniformLocationFn)(uint32_t program, const char *name);
typedef void(__stdcall *Uniform1iFn)(int location, int value);
typedef void(__stdcall *UniformVectorFn)(int location, int count, const float *value);


/* Add the info log of an object to the shader log when it has text. */
static void keep_info_log(uint32_t object)
{
    int length;

    cl_glGetInfoLogARB(object, INFO_LOG_SIZE, &length, g_info_log);
    if (0 < length)
        strcat(g_shader_log, g_info_log);
}

/* Compile one shader from its source and attach it to the program. */
static void build_shader(uint32_t program, uint32_t shader, const char *source)
{
    cl_glShaderSourceARB(shader, 1, &source, 0);
    keep_info_log(shader);
    cl_glCompileShaderARB(shader);
    keep_info_log(shader);
    cl_glAttachObjectARB(program, shader);
    keep_info_log(shader);
}

/* Compile the shaders of a program from their sources and link it. A source that is 0 is
 * skipped. The result is 1 once the program was linked, and the program is linked only once
 * when the call has at least one source. */
int __cdecl compileprogramobject(int program, const char *vertex_source, const char *fragment_source)
{
    ProgramObject *record = &g_programs[program];

    if (vertex_source != 0 || fragment_source != 0) {
        if (vertex_source != 0 && g_glext[GLEXT_VERTEX_SHADER] != 0)
            build_shader(record->program, record->vertex_shader, vertex_source);
        if (fragment_source != 0 && g_glext[GLEXT_FRAGMENT_SHADER] != 0)
            build_shader(record->program, record->fragment_shader, fragment_source);
        cl_glLinkProgramARB(record->program);
        keep_info_log(record->program);
        record->linked = 1;
        strcat(g_shader_log, "\r\n\r\n");
    }
    return record->linked;
}

/* The record holds the place of each uniform variable of the program. */
static void locate(ProgramObject *record, int index, const char *name)
{
    record->uniforms[index] = cl_glGetUniformLocationARB(record->program, name);
}

static void use_program(uint32_t program)
{
    cl_glUseProgramObjectARB(program);
}

static void set_int(const ProgramObject *record, int index, int value)
{
    cl_glUniform1iARB(record->uniforms[index], value);
}

/* Give the program the size of one pixel of the screen. */
static void set_pixel_size(const ProgramObject *record, int index)
{
    float size[2];

    size[0] = (float)(1.0 / (float)g_width);
    size[1] = (float)(1.0 / (float)g_height);
    cl_glUniform2fvARB(record->uniforms[index], 1, size);
}

static void set_atmosphere_constant(const ProgramObject *record, int index)
{
    cl_glUniform4fvARB(record->uniforms[index], 1, cl_data_atmo_constant);
}

static void set_atmosphere_wavelength(const ProgramObject *record, int index)
{
    cl_glUniform3fvARB(record->uniforms[index], 1, cl_data_atmo_wavelength);
}

/* Read the two shader files and compile the program. The caller frees the sources. */
static int compile_files(int index, const char *vertex_file, const char *fragment_file, char **vertex,
                         char **fragment)
{
    *vertex = loadsource(vertex_file);
    *fragment = loadsource(fragment_file);
    return compileprogramobject(index, *vertex, *fragment);
}

/* The sector program draws the buildings and the ground, and the window program draws the
 * windows. Both take the same variables. */
static void setup_sector(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "ooresolution");
    locate(record, 1, "viewposition");
    locate(record, 2, "atmoconstant");
    locate(record, 3, "atmowavelength");
    locate(record, 4, "worldposition");
    locate(record, 5, "lightdirection");
    locate(record, 8, "tex_color");
    locate(record, 9, "tex_shadowmap");
    set_pixel_size(record, 0);
    set_atmosphere_constant(record, 2);
    set_atmosphere_wavelength(record, 3);
    set_int(record, 8, 0);
    set_int(record, 9, 1);
    use_program(0);
}

static void setup_reflect(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "ooresolution");
    locate(record, 1, "viewposition");
    locate(record, 2, "atmoconstant");
    locate(record, 3, "atmowavelength");
    locate(record, 4, "worldposition");
    locate(record, 5, "lightdirection");
    locate(record, 8, "tex_color");
    set_atmosphere_constant(record, 2);
    set_atmosphere_wavelength(record, 3);
    set_pixel_size(record, 0);
    set_int(record, 8, 0);
    use_program(0);
}

static void setup_model(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "ooresolution");
    locate(record, 8, "tex_ambient");
    locate(record, 9, "tex_diffuse");
    locate(record, 10, "tex_shadowmap");
    set_pixel_size(record, 0);
    set_int(record, 8, 0);
    set_int(record, 9, 1);
    set_int(record, 10, 2);
    use_program(0);
}

static void setup_occlusion(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "ooresolution");
    locate(record, 8, "tex_depth");
    locate(record, 9, "tex_randomvec");
    set_pixel_size(record, 0);
    set_int(record, 8, 0);
    set_int(record, 9, 1);
    use_program(0);
}

static void setup_atmosphere(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "atmoconstant");
    locate(record, 1, "atmowavelength");
    locate(record, 2, "worldposition");
    locate(record, 3, "ooresolution");
    locate(record, 4, "lightdirection");
    set_atmosphere_constant(record, 0);
    set_atmosphere_wavelength(record, 1);
    set_pixel_size(record, 3);
    locate(record, 8, "tex_depth");
    set_int(record, 8, 0);
    use_program(0);
}

static void setup_blur(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "ooresolution");
    locate(record, 1, "sampledir");
    locate(record, 2, "blurfilter");
    locate(record, 3, "blurfilter2");
    locate(record, 8, "tex_color");
    set_pixel_size(record, 0);
    set_int(record, 8, 0);
    use_program(0);
}

static void setup_landscape(ProgramObject *record)
{
    use_program(record->program);
    locate(record, 0, "lightdirection");
    locate(record, 8, "tex_normal");
    locate(record, 9, "tex_detail");
    locate(record, 10, "tex_color");
    set_int(record, 8, 0);
    set_int(record, 9, 1);
    set_int(record, 10, 2);
    use_program(0);
}

/* A program that has no variables: only a check that it can run. */
static void setup_plain(ProgramObject *record)
{
    use_program(record->program);
    use_program(0);
}

/* Build the ten shader programs when the extension is there, and write the compile logs to
 * a file. The sources of the landscape program are not freed. */
void __cdecl setupprogramobjects(void)
{
    char *vertex;
    char *fragment;
    void *file;

    if (g_glext[GLEXT_SHADER_OBJECTS] == 0)
        return;
    g_shader_log[0] = 0;
    if (compile_files(PROGRAM_SECTOR, "shader/sector.glvs", "shader/sector.glfs", &vertex, &fragment) != 0)
        setup_sector(&g_programs[PROGRAM_SECTOR]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_WINDOW, "shader/sector_window.glvs", "shader/sector_window.glfs", &vertex,
                      &fragment) != 0)
        setup_sector(&g_programs[PROGRAM_WINDOW]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_REFLECT, "shader/reflect.glvs", "shader/reflect.glfs", &vertex, &fragment) != 0)
        setup_reflect(&g_programs[PROGRAM_REFLECT]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_MODEL, "shader/model.glvs", "shader/model.glfs", &vertex, &fragment) != 0)
        setup_model(&g_programs[PROGRAM_MODEL]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_SHADOW, "shader/shadow.glvs", "shader/shadow.glfs", &vertex, &fragment) != 0)
        setup_plain(&g_programs[PROGRAM_SHADOW]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_DEPTH, "shader/depth.glvs", "shader/depth.glfs", &vertex, &fragment) != 0)
        setup_plain(&g_programs[PROGRAM_DEPTH]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_OCCLUSION, "shader/occlusion.glvs", "shader/occlusion.glfs", &vertex, &fragment) !=
        0)
        setup_occlusion(&g_programs[PROGRAM_OCCLUSION]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_ATMOSPHERE, "shader/atmosphere.glvs", "shader/atmosphere.glfs", &vertex,
                      &fragment) != 0)
        setup_atmosphere(&g_programs[PROGRAM_ATMOSPHERE]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_BLUR, "shader/blur.glvs", "shader/blur.glfs", &vertex, &fragment) != 0)
        setup_blur(&g_programs[PROGRAM_BLUR]);
    os_free(vertex);
    os_free(fragment);
    if (compile_files(PROGRAM_LANDSCAPE, "shader/landscape.glvs", "shader/landscape.glfs", &vertex, &fragment) !=
        0)
        setup_landscape(&g_programs[PROGRAM_LANDSCAPE]);
    if (strlen(g_shader_log) != 0) {
        file = os_fopen("shaderoutput.txt", "wb");
        g_open_file = (int)file;
        if (file != 0) {
            os_fprintf(file, g_shader_log);
            os_fclose(file);
        }
    }
}
