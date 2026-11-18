/* Image loading by file or buffer, with format tests and HDR conversion. */
#ifndef CL_IMAGE_LOAD_H
#define CL_IMAGE_LOAD_H
#include "sr_image_types.h"

char *stbi_failure_reason(void);
void stbi_image_free(void *pixels);
int stbi_register_loader(void *loader);
void stbi_hdr_to_ldr_gamma(float gamma);
void stbi_hdr_to_ldr_scale(float scale);
void stbi_ldr_to_hdr_gamma(float gamma);
void stbi_ldr_to_hdr_scale(float scale);
float *ldr_to_hdr(uint8_t *data, int x, int y, unsigned comp);
uint8_t *hdr_to_ldr(float *data, int x, int y, unsigned comp);

int stbi_jpeg_test_file(void *file);
int stbi_jpeg_test_memory(const uint8_t *buffer, int length);
uint8_t *stbi_jpeg_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_jpeg_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_jpeg_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int stbi_png_test_file(void *file);
int stbi_png_test_memory(const uint8_t *buffer, int length);
uint8_t *stbi_png_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_png_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_png_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int stbi_png_info(const char *filename, int *x, int *y, int *comp);
int bmp_test(ImageReader *reader);
int stbi_bmp_test_file(void *file);
int stbi_bmp_test_memory(const uint8_t *buffer, int length);
uint8_t *stbi_bmp_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_bmp_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_bmp_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int psd_test(ImageReader *reader);
int stbi_psd_test_file(void *file);
int stbi_psd_test_memory(const uint8_t *buffer, int length);
uint8_t *stbi_psd_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_psd_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_psd_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int hdr_test(ImageReader *reader);
int stbi_hdr_test_file(void *file);
int stbi_hdr_test_memory(const uint8_t *buffer, int length);
float *stbi_hdr_load_from_file(void *file, int *x, int *y, int *comp, int requested);
float *stbi_hdr_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int stbi_tga_test_file(void *file);
int stbi_tga_test_memory(const uint8_t *buffer, int length);
uint8_t *stbi_tga_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_tga_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_tga_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);

uint8_t *stbi_load(const char *filename, int *x, int *y, int *comp, int requested);
uint8_t *stbi_load_from_file(void *file, int *x, int *y, int *comp, int requested);
uint8_t *stbi_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
float *stbi_loadf(const char *filename, int *x, int *y, int *comp, int requested);
float *stbi_loadf_from_file(void *file, int *x, int *y, int *comp, int requested);
float *stbi_loadf_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
int stbi_is_hdr_from_memory(const uint8_t *buffer, int length);
int stbi_is_hdr_from_file(void *file);
int stbi_is_hdr(const char *filename);

#endif
