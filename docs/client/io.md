# Image byte access

Image readers can use a file or a memory buffer. Files: `src/client/image/io.c`, `io.h`, `write.c` and `write.h`.

`ImageReader` stores dimensions, channel counts, a file handle and memory bounds. Byte reads return zero at the end. Memory skips and bulk reads have no bounds check. File operations use the runtime that owns the handle.

`freadswap` stores big endian items in little endian memory. It accepts item sizes one, two and four. End of file bytes become 255. The result is the requested count, including for an unsupported item size.

`write8` writes one byte. `writefv` reads one integer for each format character. Characters `1`, `2` and `4` select the number of low bytes to write in little endian order. Spaces consume no integer. An invalid character triggers an assertion and ends the write.
