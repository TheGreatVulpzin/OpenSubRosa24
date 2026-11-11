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

## What the standalone client needs

The standalone client does not run GLee. `instrumentation/client/gen_gl_slots.py` writes `build/client_image/client_gl.c`, which puts a small function in each of the 32 pointer slots of `gl_slots.csv`. The function finds the real OpenGL function on its first call. So the 1750 GLee functions of the original client (about 60 percent of its 2907 functions) have no job in the standalone client, and the group C9 needs no rewrite for it.

The extension flags (`_GLEE_...`) are the one thing that GLee gives and the slots do not. If the renderer reads one that nothing sets, the standalone client treats the extension as missing. The source in `third_party/glee/` is there for that case: compile it and call `GLeeInit` to get the flags and the pointers with the same names and values as the original client.

## Plan

1. Keep the slot functions of `gen_gl_slots.py` for the standalone client. No work for C9.
2. If a flag turns out to matter (for example `GLEE_ARB_framebuffer_object` in the renderer), compile `third_party/glee/GLee.c` into the standalone client, call `GLeeInit` after the window opens, and put the flags it needs in the client storage adapter.
3. When the renderer moves to OpenGL 3.3 or GLES (the OpenSubRosa plan), replace GLee with a modern loader.
