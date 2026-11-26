/* The functions of the system libraries that the client calls. The game ships SDL2, OpenAL and Speex as
 * DLL files. third_party/syslibs/*.def lists the names, and the build makes an import library from each. */
#ifndef CL_SYSTEM_LIBS_H
#define CL_SYSTEM_LIBS_H

#include <stddef.h>
#include <stdint.h>

/* SDL2 (window, input, timer) */
uint32_t __cdecl SDL_GetMouseState(int *, int *);
uint32_t __cdecl SDL_GetRelativeMouseState(int *, int *);
const uint8_t * __cdecl SDL_GetKeyboardState(int *);
void __cdecl SDL_PumpEvents(void);
int __cdecl SDL_GetNumDisplayModes(int);
int __cdecl SDL_GetDisplayMode(int, int, void *);
int __cdecl SDL_SetRelativeMouseMode(int);
void * __cdecl SDL_CreateWindow(const char *, int, int, int, int, uint32_t);
void __cdecl SDL_Delay(uint32_t);
void __cdecl SDL_FreeWAV(unsigned char *);
void * __cdecl SDL_GL_CreateContext(void *);
int __cdecl SDL_GL_GetAttribute(int, int *);
int __cdecl SDL_GL_SetAttribute(int, int);
int __cdecl SDL_GL_SetSwapInterval(int);
void __cdecl SDL_GL_SwapWindow(void *);
uint32_t __cdecl SDL_GetTicks(void);
void * __cdecl SDL_HapticOpenFromJoystick(void *);
int __cdecl SDL_Init(uint32_t);
int __cdecl SDL_JoystickEventState(int);
int16_t __cdecl SDL_JoystickGetAxis(void *, int);
uint8_t __cdecl SDL_JoystickGetButton(void *, int);
void * __cdecl SDL_JoystickGetGUID(void *, void *);
uint8_t __cdecl SDL_JoystickGetHat(void *, int);
int __cdecl SDL_JoystickInstanceID(void *);
const char * __cdecl SDL_JoystickNameForIndex(int);
int __cdecl SDL_JoystickNumAxes(void *);
int __cdecl SDL_JoystickNumButtons(void *);
int __cdecl SDL_JoystickNumHats(void *);
void * __cdecl SDL_JoystickOpen(int);
void __cdecl SDL_JoystickUpdate(void);
void * __cdecl SDL_LoadWAV_RW(void *, int, void *, unsigned char **, unsigned *);
int __cdecl SDL_NumJoysticks(void);
int __cdecl SDL_PollEvent(void *);
void __cdecl SDL_Quit(void);
void * __cdecl SDL_RWFromFile(const char *, const char *);
int __cdecl SDL_SetRelativeMouseMode(int);
int __cdecl SDL_SetWindowFullscreen(void *, uint32_t);
void __cdecl SDL_SetWindowSize(void *, int, int);
int __cdecl SDL_ShowCursor(int);

/* OpenAL (sound and microphone) */
void __cdecl alBufferData(unsigned, int, void *, int, int);
void __cdecl alGenBuffers(int, unsigned *);
void __cdecl alGenSources(int, unsigned *);
int __cdecl alGetError(void);
void __cdecl alGetSourcei(unsigned, int, int *);
const char * __cdecl alGetString(int);
void __cdecl alListenerf(int, float);
void __cdecl alListenerfv(int, float *);
void __cdecl alSource3f(unsigned, int, float, float, float);
void __cdecl alSourcePlay(unsigned);
void __cdecl alSourceQueueBuffers(unsigned, int, unsigned *);
void __cdecl alSourceStop(unsigned);
void __cdecl alSourceUnqueueBuffers(unsigned, int, unsigned *);
void __cdecl alSourcef(unsigned, int, float);
void __cdecl alSourcefv(unsigned, int, float *);
void __cdecl alSourcei(unsigned, int, int);
char __cdecl alcCaptureCloseDevice(void *);
void * __cdecl alcCaptureOpenDevice(char *, unsigned, int, int);
void __cdecl alcCaptureSamples(void *, void *, int);
void __cdecl alcCaptureStart(void *);
void __cdecl alcCaptureStop(void *);
char __cdecl alcCloseDevice(void *);
void * __cdecl alcCreateContext(void *, int *);
void __cdecl alcDestroyContext(void *);
void * __cdecl alcGetContextsDevice(void *);
void * __cdecl alcGetCurrentContext(void);
void __cdecl alcGetIntegerv(void *, int, int, int *);
char * __cdecl alcGetString(void *, int);
char __cdecl alcMakeContextCurrent(void *);
void * __cdecl alcOpenDevice(char *);

/* Speex (voice codec) */
void __cdecl speex_bits_destroy(void *);
void __cdecl speex_bits_init(void *);
void __cdecl speex_bits_read_from(void *, char *, int);
void __cdecl speex_bits_reset(void *);
int __cdecl speex_bits_write(void *, char *, int);
int __cdecl speex_decode(void *, void *, float *);
void __cdecl speex_decoder_destroy(void *);
void * __cdecl speex_decoder_init(void *);
int __cdecl speex_encode(void *, float *, void *);
int __cdecl speex_encoder_ctl(void *, int, void *);
void __cdecl speex_encoder_destroy(void *);
void * __cdecl speex_encoder_init(void *);
void * __cdecl speex_lib_get_mode(int);

/* Speex DSP (voice preprocessor) */
int __cdecl speex_preprocess_ctl(void *, int, void *);
int __cdecl speex_preprocess_run(void *, short *);
void __cdecl speex_preprocess_state_destroy(void *);
void * __cdecl speex_preprocess_state_init(int, int);

/* OpenGL 1.1 (the system library) */
void __stdcall glBindTexture(uint32_t target, uint32_t texture);
void __stdcall glBlendFunc(uint32_t source, uint32_t dest);
void __stdcall glClearStencil(int value);
void __stdcall glColor3fv(const float *color);
void __stdcall glColor4f(float red, float green, float blue, float alpha);
void __stdcall glColorMask(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void __stdcall glColorPointer(int size, uint32_t type, int stride, const void *pointer);
void __stdcall glCopyTexSubImage2D(uint32_t target, int level, int x_offset, int y_offset, int x, int y, int width, int height);
void __stdcall glDisableClientState(uint32_t array);
void __stdcall glDrawArrays(uint32_t mode, int first, int count);
void __stdcall glDrawElements(uint32_t mode, int count, uint32_t type, const void *indices);
void __stdcall glEnableClientState(uint32_t array);
void __stdcall glFrustum(double left, double right, double bottom, double top, double z_near, double z_far);
void __stdcall glLineWidth(float width);
void __stdcall glLoadIdentity(void);
void __stdcall glMatrixMode(uint32_t mode);
void __stdcall glMultMatrixf(const float *matrix);
void __stdcall glNormal3fv(const float *normal);
void __stdcall glNormalPointer(uint32_t type, int stride, const void *pointer);
void __stdcall glOrtho(double left, double right, double bottom, double top, double z_near, double z_far);
void __stdcall glPointSize(float size);
void __stdcall glPopMatrix(void);
void __stdcall glPushMatrix(void);
void __stdcall glReadBuffer(uint32_t buffer);
void __stdcall glReadPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *pixels);
void __stdcall glScalef(float x, float y, float z);
void __stdcall glScissor(int x, int y, int width, int height);
void __stdcall glStencilFunc(uint32_t test, int reference, uint32_t mask);
void __stdcall glStencilMask(uint32_t mask);
void __stdcall glStencilOp(uint32_t fail, uint32_t depth_fail, uint32_t pass);
void __stdcall glTexCoord2f(float s, float t);
void __stdcall glTexCoord2fv(const float *coord);
void __stdcall glTexCoordPointer(int size, uint32_t type, int stride, const void *pointer);
void __stdcall glTexImage2D(uint32_t target, int level, int internal_format, int width, int height, int border, uint32_t format, uint32_t type, const void *pixels);
void __stdcall glTexParameterf(uint32_t target, uint32_t name, float value);
void __stdcall glTexParameteri(uint32_t target, uint32_t name, int value);
void __stdcall glTranslatef(float x, float y, float z);
void __stdcall glVertex2fv(const float *vertex);
void __stdcall glVertex3f(float x, float y, float z);
void __stdcall glVertex3fv(const float *vertex);
void __stdcall glVertexPointer(int size, uint32_t type, int stride, const void *pointer);
void __stdcall glViewport(int x, int y, int width, int height);
void __stdcall glBegin(unsigned);
void __stdcall glClear(unsigned);
void __stdcall glClearColor(float, float, float, float);
void __stdcall glColor3f(float, float, float);
void __stdcall glDepthFunc(unsigned);
void __stdcall glDepthMask(unsigned char);
void __stdcall glDisable(unsigned);
void __stdcall glEnable(unsigned);
void __stdcall glEnd(void);
void __stdcall glFinish(void);
void __stdcall glFlush(void);
void __stdcall glFrontFace(unsigned);
void __stdcall glGenTextures(int, uint32_t *);
const unsigned char * __stdcall glGetString(unsigned);
void __stdcall glLightfv(unsigned, unsigned, const float *);
void __stdcall glPolygonMode(unsigned, unsigned);
void __stdcall glVertex2f(float, float);

/* Windows Sockets (ws2_32) */
unsigned __stdcall socket(int, int, int);
int __stdcall ioctlsocket(unsigned, long, unsigned long *);
int __stdcall bind(unsigned, void *, int);
uint32_t __stdcall inet_addr(const char *);
int __stdcall gethostname(char *, int);
void * __stdcall gethostbyname(const char *);
int __stdcall WSAStartup(uint16_t, void *);
int __stdcall WSACleanup(void);

/* The shell (shell32) */
void * __stdcall ShellExecuteA(void *, const char *, const char *, const char *, const char *, int);
int __stdcall SHGetSpecialFolderPathA(void *, char *, int, int);

#endif
