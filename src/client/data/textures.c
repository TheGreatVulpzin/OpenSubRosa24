/* Data of texture pages and their upload. Edit this file like any other source. */
#ifdef SR_CLIENT_NATIVE
#include "sr_client_storage.h"

char cl_data_image_name[128];
int cl_data_head_face_quads[3];
ModelTemp cl_data_modeltemp;
Image cl_data_image;
LandRender cl_data_landrender;
int cl_data_bigendian;
BoneVert cl_data_bonevert[4096];

#endif
