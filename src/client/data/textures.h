/* Declarations of the data of texture pages and their upload. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_TEXTURES_H
#define CL_DATA_TEXTURES_H
#ifndef SR_STORAGE_GUARDS
extern char cl_data_image_name[128];
extern int cl_data_head_face_quads[3];
extern ModelTemp cl_data_modeltemp;
extern Image cl_data_image;
extern LandRender cl_data_landrender;
extern int cl_data_bigendian;
extern BoneVert cl_data_bonevert[4096];
#endif
#endif
