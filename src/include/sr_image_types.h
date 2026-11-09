/* Image readers, decoders and font records. */

#ifndef SR_IMAGE_TYPES_H

#define SR_IMAGE_TYPES_H

#include <stddef.h>

#include <stdint.h>

#define SR_STATIC_ASSERT _Static_assert

#pragma pack(push, 1)

/* image_reader[]: record of 0x1c bytes. */
typedef union ImageReader { struct {
    uint32_t width;                      /* +0x0000 Image decoder and font record accesses */
    uint32_t height;                     /* +0x0004 Image decoder and font record accesses */
    int channels;                        /* +0x0008 Image decoder and font record accesses */
    int output_channels;                 /* +0x000c Image decoder and font record accesses */
    void* file;                          /* +0x0010 Image decoder and font record accesses */
    uint8_t* cursor;                     /* +0x0014 Image decoder and font record accesses */
    uint8_t* end;                        /* +0x0018 Image decoder and font record accesses */
}; uint8_t lf_raw[0x1c]; } ImageReader;
SR_STATIC_ASSERT(sizeof(ImageReader) == 0x1c, "ImageReader");
SR_STATIC_ASSERT(offsetof(ImageReader, width) == 0x0, "ImageReader.width");
SR_STATIC_ASSERT(offsetof(ImageReader, height) == 0x4, "ImageReader.height");
SR_STATIC_ASSERT(offsetof(ImageReader, channels) == 0x8, "ImageReader.channels");
SR_STATIC_ASSERT(offsetof(ImageReader, output_channels) == 0xc, "ImageReader.output_channels");
SR_STATIC_ASSERT(offsetof(ImageReader, file) == 0x10, "ImageReader.file");
SR_STATIC_ASSERT(offsetof(ImageReader, cursor) == 0x14, "ImageReader.cursor");
SR_STATIC_ASSERT(offsetof(ImageReader, end) == 0x18, "ImageReader.end");

/* png_image[]: record of 0x28 bytes. */
typedef union PngImage { struct {
    ImageReader reader;                  /* +0x0000 Image decoder and font record accesses */
    uint8_t* compressed;                 /* +0x001c Image decoder and font record accesses */
    uint8_t* expanded;                   /* +0x0020 Image decoder and font record accesses */
    uint8_t* pixels;                     /* +0x0024 Image decoder and font record accesses */
}; uint8_t lf_raw[0x28]; } PngImage;
SR_STATIC_ASSERT(sizeof(PngImage) == 0x28, "PngImage");
SR_STATIC_ASSERT(offsetof(PngImage, reader) == 0x0, "PngImage.reader");
SR_STATIC_ASSERT(offsetof(PngImage, compressed) == 0x1c, "PngImage.compressed");
SR_STATIC_ASSERT(offsetof(PngImage, expanded) == 0x20, "PngImage.expanded");
SR_STATIC_ASSERT(offsetof(PngImage, pixels) == 0x24, "PngImage.pixels");

/* zlib_huffman[]: record of 0x7e4 bytes. */
typedef union ZlibHuffman { struct {
    uint16_t fast[512];                  /* +0x0000 Image decoder and font record accesses */
    uint16_t first_code[16];             /* +0x0400 Image decoder and font record accesses */
    int max_code[17];                    /* +0x0420 Image decoder and font record accesses */
    uint16_t first_symbol[16];           /* +0x0464 Image decoder and font record accesses */
    uint8_t size[288];                   /* +0x0484 Image decoder and font record accesses */
    uint16_t value[288];                 /* +0x05a4 Image decoder and font record accesses */
}; uint8_t lf_raw[0x7e4]; } ZlibHuffman;
SR_STATIC_ASSERT(sizeof(ZlibHuffman) == 0x7e4, "ZlibHuffman");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, fast) == 0x0, "ZlibHuffman.fast");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, first_code) == 0x400, "ZlibHuffman.first_code");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, max_code) == 0x420, "ZlibHuffman.max_code");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, first_symbol) == 0x464, "ZlibHuffman.first_symbol");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, size) == 0x484, "ZlibHuffman.size");
SR_STATIC_ASSERT(offsetof(ZlibHuffman, value) == 0x5a4, "ZlibHuffman.value");

/* zlib_buffer[]: record of 0xfe8 bytes. */
typedef union ZlibBuffer { struct {
    uint8_t* cursor;                     /* +0x0000 Image decoder and font record accesses */
    uint8_t* end;                        /* +0x0004 Image decoder and font record accesses */
    int num_bits;                        /* +0x0008 Image decoder and font record accesses */
    uint32_t bits;                       /* +0x000c Image decoder and font record accesses */
    uint8_t* output;                     /* +0x0010 Image decoder and font record accesses */
    uint8_t* start;                      /* +0x0014 Image decoder and font record accesses */
    uint8_t* limit;                      /* +0x0018 Image decoder and font record accesses */
    int expandable;                      /* +0x001c Image decoder and font record accesses */
    ZlibHuffman length;                  /* +0x0020 Image decoder and font record accesses */
    ZlibHuffman distance;                /* +0x0804 Image decoder and font record accesses */
}; uint8_t lf_raw[0xfe8]; } ZlibBuffer;
SR_STATIC_ASSERT(sizeof(ZlibBuffer) == 0xfe8, "ZlibBuffer");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, cursor) == 0x0, "ZlibBuffer.cursor");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, end) == 0x4, "ZlibBuffer.end");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, num_bits) == 0x8, "ZlibBuffer.num_bits");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, bits) == 0xc, "ZlibBuffer.bits");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, output) == 0x10, "ZlibBuffer.output");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, start) == 0x14, "ZlibBuffer.start");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, limit) == 0x18, "ZlibBuffer.limit");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, expandable) == 0x1c, "ZlibBuffer.expandable");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, length) == 0x20, "ZlibBuffer.length");
SR_STATIC_ASSERT(offsetof(ZlibBuffer, distance) == 0x804, "ZlibBuffer.distance");

/* jpeg_huffman[]: record of 0x690 bytes. */
typedef union JpegHuffman { struct {
    uint8_t fast[512];                   /* +0x0000 Image decoder and font record accesses */
    uint16_t code[256];                  /* +0x0200 Image decoder and font record accesses */
    uint8_t value[256];                  /* +0x0400 Image decoder and font record accesses */
    uint8_t size[257];                   /* +0x0500 Image decoder and font record accesses */
    uint8_t pad_0601[0x3];
    uint32_t max_code[18];               /* +0x0604 Image decoder and font record accesses */
    int delta[17];                       /* +0x064c Image decoder and font record accesses */
}; uint8_t lf_raw[0x690]; } JpegHuffman;
SR_STATIC_ASSERT(sizeof(JpegHuffman) == 0x690, "JpegHuffman");
SR_STATIC_ASSERT(offsetof(JpegHuffman, fast) == 0x0, "JpegHuffman.fast");
SR_STATIC_ASSERT(offsetof(JpegHuffman, code) == 0x200, "JpegHuffman.code");
SR_STATIC_ASSERT(offsetof(JpegHuffman, value) == 0x400, "JpegHuffman.value");
SR_STATIC_ASSERT(offsetof(JpegHuffman, size) == 0x500, "JpegHuffman.size");
SR_STATIC_ASSERT(offsetof(JpegHuffman, max_code) == 0x604, "JpegHuffman.max_code");
SR_STATIC_ASSERT(offsetof(JpegHuffman, delta) == 0x64c, "JpegHuffman.delta");

/* jpeg_component[]: record of 0x38 bytes. */
typedef union JpegComponent { struct {
    int id;                              /* +0x0000 Image decoder and font record accesses */
    int h;                               /* +0x0004 Image decoder and font record accesses */
    int v;                               /* +0x0008 Image decoder and font record accesses */
    int quantization;                    /* +0x000c Image decoder and font record accesses */
    int dc_table;                        /* +0x0010 Image decoder and font record accesses */
    int ac_table;                        /* +0x0014 Image decoder and font record accesses */
    int dc_prediction;                   /* +0x0018 Image decoder and font record accesses */
    int width;                           /* +0x001c Image decoder and font record accesses */
    int height;                          /* +0x0020 Image decoder and font record accesses */
    int stride;                          /* +0x0024 Image decoder and font record accesses */
    int buffer_height;                   /* +0x0028 Image decoder and font record accesses */
    uint8_t* data;                       /* +0x002c Image decoder and font record accesses */
    void* allocation;                    /* +0x0030 Image decoder and font record accesses */
    uint8_t* line;                       /* +0x0034 Image decoder and font record accesses */
}; uint8_t lf_raw[0x38]; } JpegComponent;
SR_STATIC_ASSERT(sizeof(JpegComponent) == 0x38, "JpegComponent");
SR_STATIC_ASSERT(offsetof(JpegComponent, id) == 0x0, "JpegComponent.id");
SR_STATIC_ASSERT(offsetof(JpegComponent, h) == 0x4, "JpegComponent.h");
SR_STATIC_ASSERT(offsetof(JpegComponent, v) == 0x8, "JpegComponent.v");
SR_STATIC_ASSERT(offsetof(JpegComponent, quantization) == 0xc, "JpegComponent.quantization");
SR_STATIC_ASSERT(offsetof(JpegComponent, dc_table) == 0x10, "JpegComponent.dc_table");
SR_STATIC_ASSERT(offsetof(JpegComponent, ac_table) == 0x14, "JpegComponent.ac_table");
SR_STATIC_ASSERT(offsetof(JpegComponent, dc_prediction) == 0x18, "JpegComponent.dc_prediction");
SR_STATIC_ASSERT(offsetof(JpegComponent, width) == 0x1c, "JpegComponent.width");
SR_STATIC_ASSERT(offsetof(JpegComponent, height) == 0x20, "JpegComponent.height");
SR_STATIC_ASSERT(offsetof(JpegComponent, stride) == 0x24, "JpegComponent.stride");
SR_STATIC_ASSERT(offsetof(JpegComponent, buffer_height) == 0x28, "JpegComponent.buffer_height");
SR_STATIC_ASSERT(offsetof(JpegComponent, data) == 0x2c, "JpegComponent.data");
SR_STATIC_ASSERT(offsetof(JpegComponent, allocation) == 0x30, "JpegComponent.allocation");
SR_STATIC_ASSERT(offsetof(JpegComponent, line) == 0x34, "JpegComponent.line");

/* jpeg_image[]: record of 0x36c0 bytes. */
typedef union JpegImage { struct {
    ImageReader reader;                  /* +0x0000 Image decoder and font record accesses */
    JpegHuffman dc[4];                   /* +0x001c Image decoder and font record accesses */
    JpegHuffman ac[4];                   /* +0x1a5c Image decoder and font record accesses */
    uint8_t dequantize[256];             /* +0x349c Image decoder and font record accesses */
    int h_max;                           /* +0x359c Image decoder and font record accesses */
    int v_max;                           /* +0x35a0 Image decoder and font record accesses */
    int mcu_columns;                     /* +0x35a4 Image decoder and font record accesses */
    int mcu_rows;                        /* +0x35a8 Image decoder and font record accesses */
    int mcu_width;                       /* +0x35ac Image decoder and font record accesses */
    int mcu_height;                      /* +0x35b0 Image decoder and font record accesses */
    JpegComponent component[4];          /* +0x35b4 Image decoder and font record accesses */
    uint32_t bits;                       /* +0x3694 Image decoder and font record accesses */
    int num_bits;                        /* +0x3698 Image decoder and font record accesses */
    uint8_t marker;                      /* +0x369c Image decoder and font record accesses */
    uint8_t pad_369d[0x3];
    int no_more;                         /* +0x36a0 Image decoder and font record accesses */
    int scan_components;                 /* +0x36a4 Image decoder and font record accesses */
    int order[4];                        /* +0x36a8 Image decoder and font record accesses */
    int restart_interval;                /* +0x36b8 Image decoder and font record accesses */
    int restart_left;                    /* +0x36bc Image decoder and font record accesses */
}; uint8_t lf_raw[0x36c0]; } JpegImage;
SR_STATIC_ASSERT(sizeof(JpegImage) == 0x36c0, "JpegImage");
SR_STATIC_ASSERT(offsetof(JpegImage, reader) == 0x0, "JpegImage.reader");
SR_STATIC_ASSERT(offsetof(JpegImage, dc) == 0x1c, "JpegImage.dc");
SR_STATIC_ASSERT(offsetof(JpegImage, ac) == 0x1a5c, "JpegImage.ac");
SR_STATIC_ASSERT(offsetof(JpegImage, dequantize) == 0x349c, "JpegImage.dequantize");
SR_STATIC_ASSERT(offsetof(JpegImage, h_max) == 0x359c, "JpegImage.h_max");
SR_STATIC_ASSERT(offsetof(JpegImage, v_max) == 0x35a0, "JpegImage.v_max");
SR_STATIC_ASSERT(offsetof(JpegImage, mcu_columns) == 0x35a4, "JpegImage.mcu_columns");
SR_STATIC_ASSERT(offsetof(JpegImage, mcu_rows) == 0x35a8, "JpegImage.mcu_rows");
SR_STATIC_ASSERT(offsetof(JpegImage, mcu_width) == 0x35ac, "JpegImage.mcu_width");
SR_STATIC_ASSERT(offsetof(JpegImage, mcu_height) == 0x35b0, "JpegImage.mcu_height");
SR_STATIC_ASSERT(offsetof(JpegImage, component) == 0x35b4, "JpegImage.component");
SR_STATIC_ASSERT(offsetof(JpegImage, bits) == 0x3694, "JpegImage.bits");
SR_STATIC_ASSERT(offsetof(JpegImage, num_bits) == 0x3698, "JpegImage.num_bits");
SR_STATIC_ASSERT(offsetof(JpegImage, marker) == 0x369c, "JpegImage.marker");
SR_STATIC_ASSERT(offsetof(JpegImage, no_more) == 0x36a0, "JpegImage.no_more");
SR_STATIC_ASSERT(offsetof(JpegImage, scan_components) == 0x36a4, "JpegImage.scan_components");
SR_STATIC_ASSERT(offsetof(JpegImage, order) == 0x36a8, "JpegImage.order");
SR_STATIC_ASSERT(offsetof(JpegImage, restart_interval) == 0x36b8, "JpegImage.restart_interval");
SR_STATIC_ASSERT(offsetof(JpegImage, restart_left) == 0x36bc, "JpegImage.restart_left");

/* client_font[]: record of 0x2410 bytes. */
typedef union Font { struct {
    uint32_t resolution_x;               /* +0x0000 Image decoder and font record accesses */
    uint32_t resolution_y;               /* +0x0004 Image decoder and font record accesses */
    int texture;                         /* +0x0008 Image decoder and font record accesses */
    uint8_t pad_000c[0x4];
    float width[256];                    /* +0x0010 Image decoder and font record accesses */
    float coordinates[2048];             /* +0x0410 Image decoder and font record accesses */
}; uint8_t lf_raw[0x2410]; } Font;
SR_STATIC_ASSERT(sizeof(Font) == 0x2410, "Font");
SR_STATIC_ASSERT(offsetof(Font, resolution_x) == 0x0, "Font.resolution_x");
SR_STATIC_ASSERT(offsetof(Font, resolution_y) == 0x4, "Font.resolution_y");
SR_STATIC_ASSERT(offsetof(Font, texture) == 0x8, "Font.texture");
SR_STATIC_ASSERT(offsetof(Font, width) == 0x10, "Font.width");
SR_STATIC_ASSERT(offsetof(Font, coordinates) == 0x410, "Font.coordinates");

/* image_texture[]: record of 0x70 bytes. */
typedef union ImageTexture { struct {
    int width;                           /* +0x0000 Image decoder and font record accesses */
    int height;                          /* +0x0004 Image decoder and font record accesses */
    int levels;                          /* +0x0008 Image decoder and font record accesses */
    int format;                          /* +0x000c Image decoder and font record accesses */
    int internal_format;                 /* +0x0010 Image decoder and font record accesses */
    int wrap_s;                          /* +0x0014 Image decoder and font record accesses */
    int wrap_t;                          /* +0x0018 Image decoder and font record accesses */
    int mag_filter;                      /* +0x001c Image decoder and font record accesses */
    int min_filter;                      /* +0x0020 Image decoder and font record accesses */
    uint8_t pad_0024[0x4];
    int flags;                           /* +0x0028 Image decoder and font record accesses */
    uint32_t id;                         /* +0x002c Image decoder and font record accesses */
    uint32_t* pixels[16];                /* +0x0030 Image decoder and font record accesses */
}; uint8_t lf_raw[0x70]; } ImageTexture;
SR_STATIC_ASSERT(sizeof(ImageTexture) == 0x70, "ImageTexture");
SR_STATIC_ASSERT(offsetof(ImageTexture, width) == 0x0, "ImageTexture.width");
SR_STATIC_ASSERT(offsetof(ImageTexture, height) == 0x4, "ImageTexture.height");
SR_STATIC_ASSERT(offsetof(ImageTexture, levels) == 0x8, "ImageTexture.levels");
SR_STATIC_ASSERT(offsetof(ImageTexture, format) == 0xc, "ImageTexture.format");
SR_STATIC_ASSERT(offsetof(ImageTexture, internal_format) == 0x10, "ImageTexture.internal_format");
SR_STATIC_ASSERT(offsetof(ImageTexture, wrap_s) == 0x14, "ImageTexture.wrap_s");
SR_STATIC_ASSERT(offsetof(ImageTexture, wrap_t) == 0x18, "ImageTexture.wrap_t");
SR_STATIC_ASSERT(offsetof(ImageTexture, mag_filter) == 0x1c, "ImageTexture.mag_filter");
SR_STATIC_ASSERT(offsetof(ImageTexture, min_filter) == 0x20, "ImageTexture.min_filter");
SR_STATIC_ASSERT(offsetof(ImageTexture, flags) == 0x28, "ImageTexture.flags");
SR_STATIC_ASSERT(offsetof(ImageTexture, id) == 0x2c, "ImageTexture.id");
SR_STATIC_ASSERT(offsetof(ImageTexture, pixels) == 0x30, "ImageTexture.pixels");

#pragma pack(pop)

#endif
