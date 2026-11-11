# GLee in the client

GLee is the OpenGL extension loader that the client links in. It has 354 functions in the group C9. They are not game code, so the standalone client builds the library from its source and the lifted copy has no job.

## Which version

The client has GLee 5.4 (2009, by Ben Woodhouse, modified BSD license).

- The client lists the versions `GL_VERSION_1_2` to `GL_VERSION_3_0`, and no `GL_VERSION_3_1`. The source of GLee 5.4 stops at 3.0 as well.
- The source has 3188 lines that name a lazy function (`GLee_Lazy_gl...`, one definition and one assignment for each function): 1594 functions. `analysis/client_win/glee_names.csv` has 1595 names of lazy functions that came from the Linux client. The names are the same as in the source.
- The client has `GL_ARB_framebuffer_object`, `GL_ARB_vertex_array_object` and `GL_EXT_gpu_shader4`, which are in 5.4.

The source is in `third_party/glee/` (see `third_party/glee/README.md` for where it came from and its hash).

## How it works

- The game calls OpenGL extension functions through pointer variables (`GLeeFuncPtr_gl...`). `analysis/client_win/gl_slots.csv` lists the 32 pointers that the game code uses.
- At start, each pointer holds the address of its lazy function (`GLee_Lazy_gl...`).
- The first call of a lazy function runs `GLeeInit`. `GLeeInit` reads the extension string of the driver and calls the `__GLeeLink_...` functions. Each link function asks the driver for the real functions of one extension and writes them in the pointers. It also sets the flags (`_GLEE_...`).
- The lazy function then calls the real function through its pointer. Next calls go straight to the driver.
- `GLeeInit` needs an open OpenGL context. This is why the first call must come after the window opens.

## The standalone client

`instrumentation/client/gen_gl_slots.py` writes two files in `build/client_image/`.

- `client_gl_glee.c` puts `GLeeFuncPtr_<name>` of `third_party/glee/GLee.c` in each of the 32 slots. This is the default. The slots, the lazy functions and the flags behave as in the original client.
- `client_gl.c` also has a small function for each slot. The function finds the real OpenGL function on its first call. The environment variable `SV_GL_WRAPPERS=1` selects them. They need no GLee, so they help to find out if a problem comes from GLee.

The game never reads the `_GLEE_` flags itself. The flags matter only to the GLee code.

## Plan

1. Keep GLee for the standalone client. No rewrite for C9.
2. When the renderer moves to OpenGL 3.3 or GLES (the OpenSubRosa plan), replace GLee with a modern loader.
