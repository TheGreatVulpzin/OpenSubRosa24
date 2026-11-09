# zlib image data

The zlib decoder expands DEFLATE data for PNG images. Files: `src/client/image/zlib.c` and `zlib.h`.

`ZlibBuffer` stores input bounds, low-order bits, output bounds, an allocation flag and two `ZlibHuffman` tables. A table has 512 fast entries. Longer codes use canonical code ranges and symbol offsets.

`parse_zlib` optionally reads the zlib header, then reads stored, fixed or dynamic blocks. It rejects preset dictionaries and compression methods other than DEFLATE. Stored blocks check the length complement. Match distances cannot exceed the number of output bytes.

`fill_bits` reads until at least 25 bits are ready. End of input supplies zero bytes without moving the cursor. `zreceive` consumes the low bits. Huffman decoding returns minus one for an invalid code.

`expand` doubles the output capacity until the requested bytes fit. A fixed output buffer fails when it fills. Byte copies can overlap, so repeated patterns can extend from their own output.

`init_defaults` fills fixed code lengths. Its writes include the byte that selects whether initialization is needed. Partial decoding ends after a block supplies more than 65536 bytes.
