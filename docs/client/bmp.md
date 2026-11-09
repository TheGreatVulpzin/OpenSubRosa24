# BMP images

The BMP loader accepts core, information, extended and version-four headers. Files: `src/client/image/bmp.c` and `bmp.h`.

`bmp_load` reads indexed, 16-bit, 24-bit or 32-bit pixels. It rejects monochrome pixels and run length compression. Palettes contain up to 256 entries. Indexed rows can use four-bit or eight-bit indices.

Rows have four-byte padding. A positive height places the bottom row first, so the loader swaps rows after decoding. A negative height keeps the row order. Packed pixels use explicit masks or the default RGB masks for their depth.

Mask expansion uses the red mask bit count for all channels. Alpha defaults to 255 when no alpha mask exists. The reported channel count is the count before a final conversion to one or two channels.
