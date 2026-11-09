# Radiance images

The HDR loader produces floating-point samples from Radiance RGBE images. Files: `src/client/image/hdr.c` and `hdr.h`.

`hdr_load` requires the Radiance signature, the `32-bit_rle_rgbe` format and the negative Y, positive X orientation. A requested channel count of zero selects RGB. The reported source channel count is three.

Rows from eight to 32767 pixels can use four separate run streams. A run header must contain the image width. Commands above 128 repeat one byte. Other commands copy literal bytes. Short rows use flat RGBE pixels.

A row without a run header switches to flat input. Its first pixel goes to the start of the output image. The remaining flat pixels also start from the first row. RGBE conversion rounds the exponent scale to float before channel multiplication.
