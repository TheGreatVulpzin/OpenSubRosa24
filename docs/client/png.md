# PNG images

PNG images supply texture pixels and font atlases. Files: `src/client/image/png.c` and `png.h`.

`PngImage` contains an `ImageReader`, compressed chunk data, expanded row data and output pixels. The reader stores dimensions, source channels, output channels and the file or memory cursor.

`parse_png_file` reads the signature, dimensions, palette, transparency and image data. It reads CRC words without checking their values. It rejects unsupported critical chunks and skips ancillary chunks. The accepted sample depth is eight bits. The color types are grayscale, RGB, palette, grayscale with alpha and RGBA.

`create_png_image_raw` restores rows with None, Sub, Up, Average or Paeth prediction. The first row has zero samples above it. An added alpha channel contains 255. A full-size row buffer must have exactly the expected byte count. Partial decoding allocates one row.

`create_png_image` combines seven Adam7 passes. The input position advances by each pass width times the output channel count, plus one filter byte per row. A failed pass leaves partial decoding disabled. A successful pass restores that setting.

`compute_transparency` clears alpha when colors match. For grayscale, it also sets every nonmatching alpha to 255. For RGB, it leaves nonmatching alpha unchanged. `expand_palette` reads palette entries without checking an index against the entry count.

`convert_format` owns its input allocation. It returns that allocation when the channel count stays the same. Otherwise it allocates output storage and releases the input. RGB luminance uses integer weights 77, 150 and 29, then shifts right by eight bits.
