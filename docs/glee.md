# GLee in the client

GLee is the OpenGL extension loader that the client links in. It has 354 functions in the group C9. They are not game code, so the plan is to build the library from its source and drop the lifted copy.

## Which version

The client has GLee 5.4 (2009, by Ben Woodhouse, modified BSD license).

- The client lists the versions `GL_VERSION_1_2` to `GL_VERSION_3_0`, and no `GL_VERSION_3_1`. The source of GLee 5.4 stops at 3.0 as well.
- The source has 3188 lines that name a lazy function (`GLee_Lazy_gl...`, one definition and one assignment for each function): 1594 functions. `analysis/client_win/glee_names.csv` has 1595 names of lazy functions that came from the Linux client. The names are the same as in the source.
- The client has `GL_ARB_framebuffer_object`, `GL_ARB_vertex_array_object` and `GL_EXT_gpu_shader4`, which are in 5.4.

The source is in `third_party/glee/` (see `third_party/glee/README.md` for where it came from and its hash).

## What the client does with it

- The game calls OpenGL extension functions through pointer variables (`GLeeFuncPtr_gl...`). Before `GLeeInit` runs, a pointer points to its lazy function, which links the extension and then calls it. `analysis/client_win/gl_slots.csv` lists the 32 pointers that the game code uses.
- `GLeeInit` and the `__GLeeLink_...` functions read the extension string of the driver and fill the pointers and the flags (`_GLEE_...`).

## Plan

1. Compile `third_party/glee/GLee.c` into the standalone client, with the lifted GLee functions removed.
2. Point the game to the pointers and the flags of the compiled library. The lifted code reaches them by their address in the image, so the adapter of the client storage (`instrumentation/client/runtime/storage_adapter.h`) needs a row for each one that the game reads: the 32 pointers of `gl_slots.csv` and the flags `_GLEE_...` that `GLeeInit` sets.
3. Compare the result: the flags and the pointers after `GLeeInit` must be the same as the lifted code gives with the same extension string.
4. When the renderer moves to OpenGL 3.3 or GLES (the OpenSubRosa plan), replace GLee with a modern loader.

The group C9 then has no function to rewrite.
