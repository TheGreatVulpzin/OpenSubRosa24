/* Declarations of the data of image files and their codecs. Include sr_client_storage.h, not this file. */
#ifndef CL_DATA_IMAGE_H
#define CL_DATA_IMAGE_H
#ifndef SR_STORAGE_GUARDS
extern int cl_data_image_partial;
extern uint8_t cl_data_zlib_default_length[288];
extern uint8_t cl_data_zlib_default_distance[32];
extern char* cl_data_image_error;
extern uint8_t cl_data_font_bits[1024];
extern uint8_t cl_data_cursor_bits[9];
extern void* cl_data_image_loaders[32];
extern int cl_data_image_loader_count;
extern float cl_data_image_hdr_gamma_inverse;
extern float cl_data_image_hdr_scale_inverse;
extern float cl_data_image_ldr_gamma;
extern float cl_data_image_ldr_scale;
extern char cl_data_text_png_unknown_chunk[21];
#endif
#endif
