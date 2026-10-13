/* Operating system types that the Ghidra pseudo-C of the side-effect functions uses
 * (CRT, KERNEL32 and WinSock). The layouts are the 32-bit layouts of MSVC 2010.
 *
 * Only lifted_024d.c includes this header. Other files include <windows.h>, which has its
 * own definitions of these names.
 */
#ifndef SR_LIFT_OS_H
#define SR_LIFT_OS_H

/* FILE of MSVCR100: the lifted code only passes the pointer to the CRT. */
#ifndef _FILE_DEFINED
typedef struct _iobuf FILE;
#define _FILE_DEFINED
#endif

typedef int BOOL;
typedef void *HANDLE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long DWORD_PTR;
typedef unsigned long ULONG_PTR;
typedef DWORD_PTR *PDWORD_PTR;
typedef long long __time64_t;

/* Ghidra names the two structs of the union "s" and "u". */
typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        long HighPart;
    } s;
    struct {
        DWORD LowPart;
        long HighPart;
    } u;
    long long QuadPart;
} LARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;
typedef struct _FILETIME _FILETIME;  /* Ghidra uses the tag as a type name */

typedef struct {
    unsigned attrib;
    __time64_t time_create;
    __time64_t time_access;
    __time64_t time_write;
    unsigned long size;
    char name[260];
} _finddata64i32_t;

typedef int(__cdecl _PtFuncCompare)(const void *, const void *);

typedef unsigned int SOCKET;
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef void *LPWSADATA;

typedef struct sockaddr {
    u_short sa_family;
    char sa_data[14];
} sockaddr;

typedef struct fd_set {
    u_int fd_count;
    SOCKET fd_array[64];
} fd_set;

typedef struct timeval {
    long tv_sec;
    long tv_usec;
} timeval;

typedef struct hostent {
    char *h_name;
    char **h_aliases;
    short h_addrtype;
    short h_length;
    char **h_addr_list;
} hostent;

typedef struct servent {
    char *s_name;
    char **s_aliases;
    short s_port;
    char *s_proto;
} servent;

#endif /* SR_LIFT_OS_H */
