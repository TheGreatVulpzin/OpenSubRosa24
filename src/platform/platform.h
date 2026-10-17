/* Platform layer: the operating system services that the game uses.
 *
 * The 024d calls these services through its import table (MSVCR100, KERNEL32 and WSOCK32).
 * Each environment has one implementation of this layer:
 *
 *   plat_test.c   the offline harness: a fixed clock, a heap pool at fixed addresses,
 *                 file writes in a sandbox and no network. Both versions of a function
 *                 (lifted and original) see the same environment, and the harness compares
 *                 what they do with it.
 *   plat_imports.c  the standalone server on Windows: the static CRT, KERNEL32 and WSOCK32,
 *                 linked at build time (no MSVCR100.dll).
 *   (later)       Linux and the browser.
 *
 * An implementation keeps the ABI of the import (the same prototype and calling convention),
 * so the game code and the lifted code call it through the import table with no change.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

/* The function that replaces one import. In the harness, NULL keeps the system function. In
 * the standalone server, NULL is an unknown import. WinSock imports of the 024d are by
 * ordinal: name is NULL and ordinal is set. */
void *plat_lookup(const char *dll, const char *name, unsigned ordinal);

#endif /* PLATFORM_H */
