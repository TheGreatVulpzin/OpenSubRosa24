# Bitmap fonts

Fonts supply glyph widths and texture coordinates for text. Files: `src/client/image/fonts.c` and `fonts.h`.

Each `Font` stores a resolution, a texture index, 256 widths and eight coordinates per glyph. Each `ImageTexture` stores dimensions, sampling settings, a texture identifier and up to sixteen pixel levels.

`createfont` builds a 1024-pixel atlas with mipmaps, a 128-pixel atlas and a cursor texture. Built-in glyphs have eight rows of eight bits. Extended characters select shared bitmaps and inverse colors. Codes 28 through 31 select glyphs 124, 125, 96 and 123. Code 127 uses one texture point.

`loadfont` accepts atlas dimensions that are powers of two. Glyphs occupy a sixteen by sixteen grid. A pair of empty columns ends the width scan. Sizes above 32 start the scan two columns later. The space width uses 0.375 of a cell. Digits use the float value nearest 0.35.

Coordinates first store pixel values as floats, then divide by the atlas dimensions. Widths first divide by the cell width, then multiply by the ratio of cell width to font size. The ratio retains register precision until multiplication. Failed image loads leave metrics unchanged and retain the assigned texture index.

`setfontresolution` stores the resolution of the selected font.
