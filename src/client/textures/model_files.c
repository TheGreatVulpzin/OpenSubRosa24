/* The model files of the client (.csm): the mesh of a game model in a file of the model folder.
 *
 * The file starts with the 4 letters CSMo and the version 1. Then come the number of vertices, the
 * floats of a vertex and the number of triangles. Then come the arrays as raw floats and integers:
 * vertex places, vertex normals and vertex tangents, then the triangles, their normals and their
 * neighbors.
 */
#include <stdio.h>
#include "textures/model_files.h"

#include <string.h>

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/models.h"

#define g_temp cl_data_modeltemp
#define g_models cl_data_model

#define CSM_VERSION 1
#define CSM_MAGIC_SIZE 4
#define CSM_FOLDER "model"
#define CSM_PARENT ".."

/* Write a game model to a file of the model folder. Nothing is written when the file does not open. */
void __cdecl savemodelcsm(int model, const char *name)
{
    static const char magic[CSM_MAGIC_SIZE] = {'C', 'S', 'M', 'o'};
    const Model *m = &g_models[model];
    int version = CSM_VERSION;
    int changed = os_chdir(CSM_FOLDER);
    char path[0x80];
    void *file;

    strcpy(path, name);
    file = os_fopen(path, "wb");
    if (file != NULL) {
        os_fwrite(magic, 1, CSM_MAGIC_SIZE, file);
        os_fwrite(&version, 4, 1, file);
        os_fwrite(&m->num_verts, 4, 1, file);
        os_fwrite(&m->vertex_floats, 4, 1, file);
        os_fwrite(&m->num_tris, 4, 1, file);
        os_fwrite(m->verts, 4, (size_t)(m->num_verts * 3), file);
        os_fwrite(m->vert_normals, 4, (size_t)(m->num_verts * 3), file);
        os_fwrite(m->vert_tangents, 4, (size_t)(m->num_verts * 3), file);
        os_fwrite(m->tris, 4, (size_t)(m->num_tris * 3), file);
        os_fwrite(m->tri_normals, 4, (size_t)(m->num_tris * 3), file);
        os_fwrite(m->tri_neighbors, 4, (size_t)(m->num_tris * 3), file);
        os_fclose(file);
    }
    if (changed == 0)
        os_chdir(CSM_PARENT);
}

/* Read a model file of the model folder into the model in work and store it as a game model. The
 * result is 1 when the file has the right letters and the version 1, and 0 for every other case. The
 * counts in the file are not checked against the size of the model in work. */
int __cdecl loadmodelcsm(int model, const char *name)
{
    char magic[8];
    int version;
    int changed = os_chdir(CSM_FOLDER);
    char path[0x80];
    void *file;

    strcpy(path, name);
    file = os_fopen(path, "rb");
    if (file == NULL) {
        if (changed == 0)
            os_chdir(CSM_PARENT);
        return 0;
    }
    os_fread(magic, 1, CSM_MAGIC_SIZE, file);
    magic[CSM_MAGIC_SIZE] = '\0';
    if (strcmp(magic, "CSMo") != 0) {
        os_fclose(file);
        if (changed == 0)
            os_chdir(CSM_PARENT);
        return 0;
    }
    os_fread(&version, 4, 1, file);
    if (version != CSM_VERSION) {
        os_fclose(file);
        if (changed == 0)
            os_chdir(CSM_PARENT);
        return 0;
    }
    os_fread(&g_temp.num_verts, 4, 1, file);
    os_fread(&g_temp.vertex_floats, 4, 1, file);
    os_fread(&g_temp.num_tris, 4, 1, file);
    os_fread(g_temp.verts, 4, (size_t)(g_temp.num_verts * 3), file);
    os_fread(g_temp.vert_normals, 4, (size_t)(g_temp.num_verts * 3), file);
    os_fread(g_temp.vert_tangents, 4, (size_t)(g_temp.num_verts * 3), file);
    os_fread(g_temp.tris, 4, (size_t)(g_temp.num_tris * 3), file);
    os_fread(g_temp.tri_normals, 4, (size_t)(g_temp.num_tris * 3), file);
    os_fread(g_temp.tri_neighbors, 4, (size_t)(g_temp.num_tris * 3), file);
    os_fclose(file);
    if (changed == 0)
        os_chdir(CSM_PARENT);
    setupmodel(model, 0);
    return 1;
}
