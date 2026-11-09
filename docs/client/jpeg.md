# JPEG images

The JPEG loader accepts baseline grayscale and three-component frames. Files: `src/client/image/jpeg.c` and `jpeg.h`.

`JpegImage` contains the reader, four DC tables, four AC tables, quantization values and four component records. `JpegComponent` stores sampling factors, table indices, a DC predictor, dimensions and pixel storage. Only one or three components are accepted in a frame.

`decode_jpeg_header` reads markers until it finds a baseline frame. Progressive frames and sixteen-bit quantization tables fail. `process_frame_header` computes MCU dimensions and allocates each component with sixteen-byte alignment. Header-only reads allocate no component pixels.

`decode_block` restores DC differences and AC runs in zigzag order. `parse_entropy_coded_data` stores blocks for one component or an interleaved scan. Restart boundaries clear the bit buffer and three DC predictors. A missing restart marker ends the scan.

`idct_block` uses twelve-bit integer cosine constants. Columns add 512 before a right shift of ten bits. Rows add 65536 before a right shift of seventeen bits. Samples then add 128 and clamp to the byte range. A column with no AC terms uses its DC term directly.

`YCbCr_to_RGB_row` converts chroma differences with integer coefficients. Every pixel writes four bytes, including opaque alpha, even when the pixel stride is three. `cleanup_jpeg` releases component allocations and row buffers. It clears active pointers and retains the other component fields.
