/* DEFLATE decoding for compressed image data.
 * Blocks use fixed codes, dynamic codes or bytes without compression.
 * The caller supplies an output buffer and selects whether it can grow.
 */
#include "image/zlib.h"
#include <string.h>
#include "image/io.h"
#include "sr_runtime.h"

enum { ZLIB_FAST_BITS = 9, ZLIB_FAST_SIZE = 1 << ZLIB_FAST_BITS,
       ZLIB_CODE_BITS = 15, ZLIB_LENGTH_CODES = 288, ZLIB_DISTANCE_CODES = 32,
       ZLIB_END_BLOCK = 256, ZLIB_FIRST_LENGTH = 257, ZLIB_PARTIAL_SIZE = 65536 };

enum {
    ZLIB_ASSERT_CODE_COUNT = 0x11152c,
    ZLIB_CODE_SOURCE = 0x111600,
    ZLIB_ERROR_CODE_LENGTHS = 0x1115dc,
    ZLIB_ASSERT_BITS = 0x111718,
    ZLIB_BITS_SOURCE = 0x111698,
    ZLIB_ASSERT_CODE_SIZE = 0x11166c,
    ZLIB_DECODE_SOURCE = 0x111778,
    ZLIB_ERROR_OUTPUT_LIMIT = 0x1115ec,
    ZLIB_ERROR_MEMORY = 0x11168c,
    ZLIB_ERROR_LITERAL = 0x111704,
    ZLIB_ERROR_DISTANCE_CODE = 0x111764,
    ZLIB_ERROR_DISTANCE = 0x1117e4,
    ZLIB_ASSERT_CODE_RANGE = 0x1118fc,
    ZLIB_DYNAMIC_SOURCE = 0x111a20,
    ZLIB_ASSERT_REPEAT = 0x111a8c,
    ZLIB_REPEAT_SOURCE = 0x111ac0,
    ZLIB_ERROR_REPEAT = 0x111b2c,
    ZLIB_ASSERT_ALIGNMENT = 0x111a9c,
    ZLIB_STORED_SOURCE = 0x111b50,
    ZLIB_ERROR_LENGTH = 0x111bbc,
    ZLIB_ERROR_INPUT = 0x111b3c,
    ZLIB_ERROR_HEADER = 0x111bcc,
    ZLIB_ERROR_DICTIONARY = 0x111bdc,
    ZLIB_ERROR_METHOD = 0x111bec
};

/* Reverse the low sixteen bits. */
int bitreverse16(uint32_t value)
{
    value = ((value & 0xaaaau) >> 1) | ((value & 0x5555u) << 1);
    value = ((value & 0xccccu) >> 2) | ((value & 0x3333u) << 2);
    value = ((value & 0xf0f0u) >> 4) | ((value & 0x0f0fu) << 4);
    return (int)(((value & 0xff00u) >> 8) | ((value & 0xffu) << 8));
}

/* Build canonical codes. Return zero when a length exceeds its code space. */
int zbuild_huffman(ZlibHuffman *table, const uint8_t *sizes, int count)
{
    int frequency[ZLIB_CODE_BITS + 2] = {0};
    int next[ZLIB_CODE_BITS + 2];
    int code = 0, total = 0, length, symbol, slot, fast;
    memset(table->fast, 0xff, sizeof table->fast);
    for (symbol = 0; symbol < count; symbol++)
        frequency[sizes[symbol]]++;
    frequency[0] = 0;
    for (length = 1; length <= ZLIB_CODE_BITS; length++)
        image_check(frequency[length] <= (1 << length), ZLIB_ASSERT_CODE_COUNT, ZLIB_CODE_SOURCE, 0x786);
    for (length = 1; length <= ZLIB_CODE_BITS; length++) {
        next[length] = code;
        table->first_code[length] = (uint16_t)code;
        table->first_symbol[length] = (uint16_t)total;
        code += frequency[length];
        if (frequency[length] && code - 1 >= (1 << length))
            return image_fail(ZLIB_ERROR_CODE_LENGTHS);
        table->max_code[length] = code << (16 - length);
        code *= 2;
        total += frequency[length];
    }
    table->max_code[16] = 65536;
    for (symbol = 0; symbol < count; symbol++) {
        length = sizes[symbol];
        if (!length)
            continue;
        slot = next[length] - table->first_code[length] + table->first_symbol[length];
        table->size[slot] = (uint8_t)length;
        table->value[slot] = (uint16_t)symbol;
        if (length <= ZLIB_FAST_BITS)
            for (fast = bitreverse16(next[length]) >> (16 - length);
                 fast < ZLIB_FAST_SIZE; fast += 1 << length)
                table->fast[fast] = (uint16_t)slot;
        next[length]++;
    }
    return 1;
}

/* Read one byte. Return zero at the end without moving the cursor. */
uint8_t zstbi_get8(ZlibBuffer *buffer)
{
    return buffer->cursor < buffer->end ? *buffer->cursor++ : 0;
}

/* Fill the low bits until at least twenty-five bits are ready. */
void fill_bits(ZlibBuffer *buffer)
{
    do {
        image_check(buffer->bits < (1u << (buffer->num_bits & 31)), ZLIB_ASSERT_BITS, ZLIB_BITS_SOURCE, 0x7c4);
        buffer->bits |= (uint32_t)zstbi_get8(buffer) << (buffer->num_bits & 31);
        buffer->num_bits += 8;
    } while (buffer->num_bits < 25);
}

/* Consume count low bits, refilling as needed. */
unsigned zreceive(ZlibBuffer *buffer, int count)
{
    uint32_t value;
    if (buffer->num_bits < count)
        fill_bits(buffer);
    value = buffer->bits;
    buffer->bits >>= count & 31;
    buffer->num_bits -= count;
    return value & ((1u << (count & 31)) - 1);
}

/* Read a canonical symbol. Return minus one for an invalid code. */
int zhuffman_decode(ZlibBuffer *buffer, const ZlibHuffman *table)
{
    unsigned slot, length;
    int reversed;
    if (buffer->num_bits < 16)
        fill_bits(buffer);
    slot = table->fast[buffer->bits & (ZLIB_FAST_SIZE - 1)];
    if (slot < 0xffff) {
        length = table->size[slot];
    } else {
        reversed = bitreverse16(buffer->bits);
        for (length = ZLIB_FAST_BITS + 1; reversed >= table->max_code[length]; length++)
            ;
        if (length == 16)
            return -1;
        slot = (reversed >> (16 - length)) - table->first_code[length] + table->first_symbol[length];
        image_check(table->size[slot] == length, ZLIB_ASSERT_CODE_SIZE, ZLIB_DECODE_SOURCE, 0x7e9);
    }
    buffer->bits >>= length;
    buffer->num_bits -= length;
    return table->value[slot];
}

/* Grow output storage by powers of two. Return zero for a fixed or full buffer. */
int expand(ZlibBuffer *buffer, int count)
{
    int used = (int)(buffer->output - buffer->start);
    size_t size = buffer->limit - buffer->start;
    uint8_t *data;
    if (!buffer->expandable)
        return image_fail(ZLIB_ERROR_OUTPUT_LIMIT);
    while ((int)size < used + count)
        size *= 2;
    data = image_realloc(buffer->start, size);
    if (!data)
        return image_fail(ZLIB_ERROR_MEMORY);
    buffer->start = data;
    buffer->output = data + used;
    buffer->limit = data + size;
    return 1;
}

/* Decode literals and overlapping copies. Return zero for bad codes or distances. */
int parse_huffman_block(ZlibBuffer *buffer)
{
    static const int length_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
        35,43,51,59,67,83,99,115,131,163,195,227,258,0,0};
    static const int length_bits[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
        3,3,3,3,4,4,4,4,5,5,5,5,0,0,0};
    static const int distance_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
        257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};
    static const int distance_bits[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
        7,7,8,8,9,9,10,10,11,11,12,12,13,13,0,0};
    for (;;) {
        int symbol = zhuffman_decode(buffer, &buffer->length);
        int count, distance;
        uint8_t *source;
        if (symbol < 0)
            return image_fail(ZLIB_ERROR_LITERAL);
        if (symbol < ZLIB_END_BLOCK) {
            if (buffer->output >= buffer->limit && !expand(buffer, 1))
                return 0;
            *buffer->output++ = (uint8_t)symbol;
            continue;
        }
        if (symbol == ZLIB_END_BLOCK)
            return 1;
        symbol -= ZLIB_FIRST_LENGTH;
        count = length_base[symbol];
        if (length_bits[symbol])
            count += zreceive(buffer, length_bits[symbol]);
        symbol = zhuffman_decode(buffer, &buffer->distance);
        if (symbol < 0)
            return image_fail(ZLIB_ERROR_DISTANCE_CODE);
        distance = distance_base[symbol];
        if (distance_bits[symbol])
            distance += zreceive(buffer, distance_bits[symbol]);
        if (buffer->output - buffer->start < distance)
            return image_fail(ZLIB_ERROR_DISTANCE);
        if (buffer->output + count > buffer->limit && !expand(buffer, count))
            return 0;
        source = buffer->output - distance;
        while (count--)
            *buffer->output++ = *source++;
    }
}

/* Read dynamic code lengths. Return zero for a bad tree or repeat count. */
int compute_huffman_codes(ZlibBuffer *buffer)
{
    static const uint8_t order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint8_t sizes[ZLIB_LENGTH_CODES + ZLIB_DISTANCE_CODES + 137];
    uint8_t code_sizes[19] = {0};
    ZlibHuffman code;
    int lengths = zreceive(buffer, 5) + ZLIB_FIRST_LENGTH;
    int distances = zreceive(buffer, 5) + 1;
    int codes = zreceive(buffer, 4) + 4;
    int index = 0, symbol, repeat, value;
    for (index = 0; index < codes; index++)
        code_sizes[order[index]] = (uint8_t)zreceive(buffer, 3);
    if (!zbuild_huffman(&code, code_sizes, 19))
        return 0;
    index = 0;
    while (index < lengths + distances) {
        symbol = zhuffman_decode(buffer, &code);
        image_check(symbol >= 0 && symbol < 19, ZLIB_ASSERT_CODE_RANGE, ZLIB_DYNAMIC_SOURCE, 0x840);
        if (symbol < 16) {
            sizes[index++] = (uint8_t)symbol;
            continue;
        }
        value = symbol == 16 ? sizes[index - 1] : 0;
        if (symbol != 16 && symbol != 17)
            image_check(symbol == 18, ZLIB_ASSERT_REPEAT, ZLIB_REPEAT_SOURCE, 0x84c);
        repeat = symbol == 16 ? zreceive(buffer, 2) + 3 :
                 symbol == 17 ? zreceive(buffer, 3) + 3 : zreceive(buffer, 7) + 11;
        memset(sizes + index, value, repeat);
        index += repeat;
    }
    if (index != lengths + distances)
        return image_fail(ZLIB_ERROR_REPEAT);
    return zbuild_huffman(&buffer->length, sizes, lengths) &&
           zbuild_huffman(&buffer->distance, sizes + lengths, distances);
}

/* Copy an aligned block. Return zero for a bad length or short input. */
int parse_uncompressed_block(ZlibBuffer *buffer)
{
    uint8_t header[4];
    unsigned count, complement;
    int index = 0;
    if (buffer->num_bits & 7)
        zreceive(buffer, buffer->num_bits & 7);
    while (buffer->num_bits > 0) {
        header[index++] = (uint8_t)buffer->bits;
        buffer->bits >>= 8;
        buffer->num_bits -= 8;
    }
    image_check(buffer->num_bits == 0, ZLIB_ASSERT_ALIGNMENT, ZLIB_STORED_SOURCE, 0x865);
    while (index < 4)
        header[index++] = zstbi_get8(buffer);
    count = header[0] + (header[1] << 8);
    complement = header[2] + (header[3] << 8);
    if (complement != (count ^ 0xffff))
        return image_fail(ZLIB_ERROR_LENGTH);
    if (buffer->cursor + count > buffer->end)
        return image_fail(ZLIB_ERROR_INPUT);
    if (buffer->output + count > buffer->limit && !expand(buffer, count))
        return 0;
    memcpy(buffer->output, buffer->cursor, count);
    buffer->cursor += count;
    buffer->output += count;
    return 1;
}

/* Read the zlib header. Reject dictionaries and methods other than DEFLATE. */
int parse_zlib_header(ZlibBuffer *buffer)
{
    unsigned method = zstbi_get8(buffer), flags = zstbi_get8(buffer);
    if ((method * 256 + flags) % 31)
        return image_fail(ZLIB_ERROR_HEADER);
    if (flags & 32)
        return image_fail(ZLIB_ERROR_DICTIONARY);
    if ((method & 15) != 8)
        return image_fail(ZLIB_ERROR_METHOD);
    return 1;
}

/* Set the code lengths of fixed blocks. */
void init_defaults(void)
{
    memset(cl_data_zlib_default_length, 8, 144);
    memset(cl_data_zlib_default_length + 144, 9, 112);
    memset(cl_data_zlib_default_length + 256, 7, 24);
    memset(cl_data_zlib_default_length + 280, 8, 8);
    memset(cl_data_zlib_default_distance, 5, ZLIB_DISTANCE_CODES);
}

/* Decode blocks. Return zero for bad input or insufficient output storage. */
int parse_zlib(ZlibBuffer *buffer, int header)
{
    int final, type;
    if (header && !parse_zlib_header(buffer))
        return 0;
    buffer->num_bits = 0;
    buffer->bits = 0;
    do {
        final = zreceive(buffer, 1);
        type = zreceive(buffer, 2);
        if (!type) {
            if (!parse_uncompressed_block(buffer))
                return 0;
        } else {
            if (type == 3)
                return 0;
            if (type == 1) {
                if (!cl_data_zlib_defaults_ready)
                    init_defaults();
                if (!zbuild_huffman(&buffer->length, cl_data_zlib_default_length, ZLIB_LENGTH_CODES) ||
                    !zbuild_huffman(&buffer->distance, cl_data_zlib_default_distance, ZLIB_DISTANCE_CODES))
                    return 0;
            } else if (!compute_huffman_codes(buffer)) {
                return 0;
            }
            if (!parse_huffman_block(buffer))
                return 0;
        }
    } while (!final && (!cl_data_image_partial || buffer->output - buffer->start < ZLIB_PARTIAL_SIZE + 1));
    return 1;
}
