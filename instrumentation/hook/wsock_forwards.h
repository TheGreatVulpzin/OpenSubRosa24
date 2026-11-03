/* The 47 WinSock 1.1 functions of wsock32.dll that the proxies forward: X(ordinal, name). */
#ifndef SV_WSOCK_FORWARDS_H
#define SV_WSOCK_FORWARDS_H

#define SV_FORWARDS(X)                                                              \
    X(1, accept) X(2, bind) X(3, closesocket) X(4, connect) X(5, getpeername)      \
    X(6, getsockname) X(7, getsockopt) X(8, htonl) X(9, htons) X(10, inet_addr)    \
    X(11, inet_ntoa) X(12, ioctlsocket) X(13, listen) X(14, ntohl) X(15, ntohs)    \
    X(16, recv) X(17, recvfrom) X(18, select) X(19, send) X(20, sendto)            \
    X(21, setsockopt) X(22, shutdown) X(23, socket) X(51, gethostbyaddr)           \
    X(52, gethostbyname) X(53, getprotobyname) X(54, getprotobynumber)             \
    X(55, getservbyname) X(56, getservbyport) X(57, gethostname)                   \
    X(101, WSAAsyncSelect) X(102, WSAAsyncGetHostByAddr)                           \
    X(103, WSAAsyncGetHostByName) X(104, WSAAsyncGetProtoByNumber)                 \
    X(105, WSAAsyncGetProtoByName) X(106, WSAAsyncGetServByPort)                   \
    X(107, WSAAsyncGetServByName) X(108, WSACancelAsyncRequest)                    \
    X(109, WSASetBlockingHook) X(110, WSAUnhookBlockingHook)                       \
    X(111, WSAGetLastError) X(112, WSASetLastError) X(113, WSACancelBlockingCall)  \
    X(114, WSAIsBlocking) X(115, WSAStartup) X(116, WSACleanup) X(151, __WSAFDIsSet)

#endif
