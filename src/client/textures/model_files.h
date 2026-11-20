/* The model files of the client. See model_files.c. */
#ifndef CL_TEXTURES_MODEL_FILES_H
#define CL_TEXTURES_MODEL_FILES_H

#include "sr_types.h"

void __cdecl savemodelcsm(int model, const char *name);
int __cdecl loadmodelcsm(int model, const char *name);

#endif /* CL_TEXTURES_MODEL_FILES_H */
