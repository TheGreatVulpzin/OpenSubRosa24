# Image loading

Every texture file goes through these functions. Files: `src/client/image/load.c` and `load.h`.

`stbi_load`, `stbi_load_from_file` and `stbi_load_from_memory` find the format from the content. The order of the tests is JPEG, PNG, BMP, PSD, HDR, the registered loaders and TGA last. The TGA test accepts many files, so it goes last. A test reads from the start of the file and puts the file back where it was.

The channel count `requested` selects the channels of the result. A value of 0 keeps the channels of the file. The pointer `comp` receives the channels of the file.

An HDR file gives floats. `hdr_to_ldr` converts them to bytes with the inverse gamma and the inverse scale, then rounds and cuts to 0 and 255. `ldr_to_hdr` goes the other way for `stbi_loadf`. The gamma and scale functions (`stbi_hdr_to_ldr_gamma`, `stbi_ldr_to_hdr_scale` and the others) set these values.

`stbi_register_loader` adds a loader with its tests and loads to a list of 32 entries. It returns 0 when the list is full.

`stbi_failure_reason` returns the text of the last failure. Failures store a pointer to a text in the data of the executable. The loaders return null and do not free the file.
