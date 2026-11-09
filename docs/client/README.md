# Image decoders and fonts

The image loaders supply texture pixels and font atlases. Each format uses a separate source file and header in `src/client/image/`.

- [PNG](png.md): chunks, row filters, palettes and transparency.
- [zlib](zlib.md): DEFLATE blocks and Huffman codes.
- [JPEG](jpeg.md): baseline frames, component blocks and color conversion.
- [BMP](bmp.md): palettes and packed color masks.
- [PSD](psd.md): planar RGB channels and PackBits runs.
- [HDR](hdr.md): Radiance RGBE samples.
- [Fonts](fonts.md): bitmap atlases, widths and coordinates.
- [Byte access](io.md): memory readers and file fields.
