/* Win32 and OpenGL types that the Ghidra prototypes of the client use.
 * The lifted client C includes this file after lift_os.h. */
#ifndef SR_LIFT_OS_CLIENT_H
#define SR_LIFT_OS_CLIENT_H

typedef const char *LPCSTR;
typedef char *LPSTR;
typedef const unsigned short *LPCWSTR;
typedef unsigned short *LPWSTR;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef long LONG;
typedef long LRESULT;
typedef unsigned int WPARAM;
typedef long LPARAM;
typedef void *HWND;
typedef void *HDC;
typedef void *HGLRC;
typedef void *HINSTANCE;
typedef void *HMODULE;
typedef void *HICON;
typedef void *HCURSOR;
typedef void *HMENU;
typedef int(__stdcall *PROC)(void);

/* Open a web page in the default browser. The standalone client does it. The harness opens nothing. */
#ifdef SR_STANDALONE
void sr_open_url(const char *url);
#else
static void sr_open_url(const char *url)
{
    (void)url;
}
#endif

#endif
