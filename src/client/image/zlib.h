/* DEFLATE streams with optional zlib headers. */
#ifndef CL_IMAGE_ZLIB_H
#define CL_IMAGE_ZLIB_H

#include "sr_image_types.h"

int bitreverse16(uint32_t value);
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

#endif
