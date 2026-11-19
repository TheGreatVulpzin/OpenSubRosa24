# TGA images

TGA images supply texture pixels. Files: `src/client/image/tga.c` and `tga.h`.

`tga_test` reads the header and accepts image types 1, 2, 3, 9, 10 and 11. The color map type must be 0 or 1. Width and height must be at least 1. The pixel size must be 8, 16, 24 or 32 bits. It reads from the start and the caller puts the file back.

`tga_load` reads the full header, skips the image id and reads the palette when the file has one. A palette file uses the palette entry size as the pixel size. A palette index past the entry count selects entry 0. Types 9 to 11 use run length rows. A command byte gives a count of 1 to 128. The top bit selects one repeated pixel or a list of raw pixels.

The channel count of the result is the requested count. A request outside 1 to 4 gives the channel count of the file. One channel and two channels use the luminance function of the PNG code. Pixel bytes arrive as blue, green, red and alpha, and the loader stores red, green, blue and alpha. A file that has bit 5 of the descriptor clear is turned over. The loader gives up on a bad header with no pixels and no message.
