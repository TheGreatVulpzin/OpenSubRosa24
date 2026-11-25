/* The start of the client. The data of the game are C objects, so the start only gives them
 * their first values, sets the math mode and runs the game.
 *
 * SR_SAMPLE=1 prints where the main thread is once a second. Map the addresses with the .map
 * file of the link (tools/re/map_address.py). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sr_math.h"
#include "sr_runtime.h"

#ifdef SR_SMOKE
#include "client_smoke.h"
#endif

void cl_storage_init(void);
int __cdecl client_main(void);

static HANDLE g_main_thread;

/* A word of the stack that looks like the return address of a call. */
static int is_return_address(uintptr_t word, uintptr_t base, uintptr_t end)
{
    const uint8_t *code = (const uint8_t *)word;
    if (word < base + 8 || word >= end)
        return 0;
    return code[-5] == 0xE8 || code[-2] == 0xFF || code[-6] == 0xFF;
}

static DWORD WINAPI sampler(LPVOID unused)
{
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    (void)unused;
    for (;;) {
        CONTEXT context;
        Sleep(1000);
        SuspendThread(g_main_thread);
        context.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(g_main_thread, &context)) {
            const uintptr_t *stack = (const uintptr_t *)context.Esp;
            int i, shown = 0;
            printf("SAMPLE base=%p eip=%p calls:", (void *)base, (void *)context.Eip);
            for (i = 0; i < 2048 && shown < 10; i++)
                if (!IsBadReadPtr(stack + i, sizeof *stack) && is_return_address(stack[i], base, end)) {
                    printf(" %p", (void *)stack[i]);
                    shown++;
                }
            printf("\n");
        }
        ResumeThread(g_main_thread);
    }
}

int main(void)
{
    unsigned control;

    setvbuf(stdout, NULL, _IONBF, 0);
    /* The game uses 53 bit x87 math. */
    _controlfp_s(&control, _PC_53, _MCW_PC);
    cl_storage_init();
    x87_init();
    /* SR_DEBUGIT=1 shows the editors in the main menu. */
    if (getenv("SR_DEBUGIT") && atoi(getenv("SR_DEBUGIT")) != 0)
        sr_data_debugit = 1;
    if (getenv("SR_SAMPLE") && atoi(getenv("SR_SAMPLE")) != 0) {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE,
                        DUPLICATE_SAME_ACCESS);
        CreateThread(NULL, 0, sampler, NULL, 0, NULL);
    }
#ifdef SR_SMOKE
    if (!client_smoke_start())
        return 2;
    client_main();
    return client_smoke_finish();
#else
    return client_main();
#endif
}
