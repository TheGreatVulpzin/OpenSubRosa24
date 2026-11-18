/* DEFLATE streams with optional zlib headers. */
#ifndef CL_IMAGE_ZLIB_H
#define CL_IMAGE_ZLIB_H

#include "sr_image_types.h"

int bitreverse16(uint32_t value);
int stbi_bit_reverse(int value, int bits);
int zbuild_huffman(ZlibHuffman *table, const uint8_t *sizes, int count);
unsigned zreceive(ZlibBuffer *buffer, int count);
void fill_bits(ZlibBuffer *buffer);
uint8_t zstbi_get8(ZlibBuffer *buffer);
int zhuffman_decode(ZlibBuffer *buffer, const ZlibHuffman *table);
int expand(ZlibBuffer *buffer, int count);
int parse_huffman_block(ZlibBuffer *buffer);
int compute_huffman_codes(ZlibBuffer *buffer);
int parse_uncompressed_block(ZlibBuffer *buffer);
int parse_zlib_header(ZlibBuffer *buffer);
void init_defaults(void);
int parse_zlib(ZlibBuffer *buffer, int header);
int do_zlib(ZlibBuffer *buffer, uint8_t *output, int length, int expandable, int header);
uint8_t *stbi_zlib_decode_malloc_guesssize(const uint8_t *input, int length, int guess, int *out_length);
uint8_t *stbi_zlib_decode_malloc(const uint8_t *input, int length, int *out_length);
int stbi_zlib_decode_buffer(uint8_t *output, int output_length, const uint8_t *input, int input_length);
uint8_t *stbi_zlib_decode_noheader_malloc(const uint8_t *input, int length, int *out_length);
int stbi_zlib_decode_noheader_buffer(uint8_t *output, int output_length, const uint8_t *input, int input_length);

#endif
