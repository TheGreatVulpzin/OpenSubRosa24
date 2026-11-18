# OpenGL loaders and stencil

`initWGLLoadFunctions` fills 37 WGL slots with resolver entry points. Each resolver can replace its own slot with the selected extension function.

`settwosidedstencil` selects the EXT implementation first. It sets the back face, then the front face, with a full write mask for each.
The ATI implementation uses separate front and back comparison functions and two separate operation calls. It sets the write mask once.
Without either extension, the function makes no OpenGL call.
