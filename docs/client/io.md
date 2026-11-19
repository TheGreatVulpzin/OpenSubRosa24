# Image byte access

Image readers can use a file or a memory buffer. Files: `src/client/image/io.c`, `io.h`, `write.c` and `write.h`.

`ImageReader` stores dimensions, channel counts, a file handle and memory bounds. Byte reads return zero at the end. Memory skips and bulk reads have no bounds check. File operations use the runtime that owns the handle.

`freadswap` stores big endian items in little endian memory. It accepts item sizes one, two and four. End of file bytes become 255. The result is the requested count, including for an unsupported item size.

`write8` writes one byte. `writefv` reads one integer for each format character. Characters `1`, `2` and `4` select the number of low bytes to write in little endian order. Spaces consume no integer. An invalid character triggers an assertion and ends the write.

`write_pixels` writes the rows of an image after the header. Each row ends with a number of zero bytes that the caller gives. A negative `vdir` writes the bottom row first. `rgb_dir` selects blue, green, red or red, green, blue. One and two channels give gray in three bytes. Four channels mix with a pink background, unless the alpha argument is not zero. A negative alpha argument puts the last channel before the color and a positive one puts it after.

`outfile` opens a file, writes the header fields with the format string and then calls `write_pixels`. It returns 1 when the file opens. The header values come from the variable arguments, so the function reads them from the stack after the format. `stbi_write_bmp` uses it to write a 24 bit BMP file with rows padded to four bytes.
