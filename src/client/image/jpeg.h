/* Baseline JPEG headers, entropy codes and pixel blocks. */
#ifndef CL_IMAGE_JPEG_H
#define CL_IMAGE_JPEG_H

#include "sr_image_types.h"

int at_eof(ImageReader *reader);
int decode_jpeg_image(JpegImage *image);
int parse_entropy_coded_data(JpegImage *image);
void grow_buffer_unsafe(JpegImage *image);
int decode_block(JpegImage *image, int16_t *block, JpegHuffman *dc, JpegHuffman *ac, int component);
int decode(JpegImage *image, const JpegHuffman *table);
unsigned extend_receive(JpegImage *image, int count);
void idct_block(uint8_t *output, int stride, const int16_t *data, const uint8_t *quantization);
unsigned process_marker(JpegImage *image, int marker);
int build_huffman(JpegHuffman *table, const int *counts);
int process_scan_header(JpegImage *image);
int decode_jpeg_header(JpegImage *image, int scan);
int process_frame_header(JpegImage *image, int scan);
void YCbCr_to_RGB_row(uint8_t *output, const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
                      int count, int stride);
void cleanup_jpeg(JpegImage *image);
void __cdecl reset(JpegImage *image);

#endif
