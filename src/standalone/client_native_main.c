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

HANDLE g_main_thread;

/* A word of the stack that looks like the return address of a call. */
static int is_return_address(uintptr_t word, uintptr_t base, uintptr_t end)
{
    const uint8_t *code = (const uint8_t *)word;
    if (word < base + 8 || word >= end)
        return 0;
    return code[-5] == 0xE8 || code[-2] == 0xFF || code[-6] == 0xFF;
}

/* Print where the main thread is: its address and the return addresses on its stack. */
static void print_main_thread(void)
{
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    CONTEXT context;

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

static DWORD WINAPI sampler(LPVOID unused)
{
    /* SR_SAMPLE_MS sets the time between two samples. The default is one second. */
    const char *text = getenv("SR_SAMPLE_MS");
    DWORD interval = text && atoi(text) > 0 ? (DWORD)atoi(text) : 1000;

    (void)unused;
    for (;;) {
        Sleep(interval);
        print_main_thread();
    }
}

#ifdef SR_STORAGE_GUARDS
const char *cl_guards_scan(unsigned *first, unsigned *last);

/* Each object has a guard after it. When a guard changes, some code wrote past the end of the
 * object: print its name and where the main thread is. */
static DWORD WINAPI guard_watcher(LPVOID unused)
{
    (void)unused;
    for (;;) {
        unsigned first, last;
        const char *name = cl_guards_scan(&first, &last);

        if (name) {
            printf("GUARD %s first=%u last=%u\n", name, first, last);
            print_main_thread();
        }
        Sleep(1);
    }
}
#endif

/* Print where a crash happened: the address, the faulting access and the return addresses on the
 * stack. Map the addresses with the .map file of the link. */
static LONG WINAPI crash_report(EXCEPTION_POINTERS *info)
{
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    const uintptr_t *stack = (const uintptr_t *)info->ContextRecord->Esp;
    int i, shown = 0;

    printf("CRASH code=%08lx base=%p eip=%p", info->ExceptionRecord->ExceptionCode, (void *)base,
           (void *)info->ContextRecord->Eip);
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
        printf(" access=%lu address=%p", (unsigned long)info->ExceptionRecord->ExceptionInformation[0],
               (void *)info->ExceptionRecord->ExceptionInformation[1]);
    printf("\nCRASH calls:");
    for (i = 0; i < 4096 && shown < 16; i++)
        if (!IsBadReadPtr(stack + i, sizeof *stack) && is_return_address(stack[i], base, end)) {
            printf(" %p", (void *)stack[i]);
            shown++;
        }
    printf("\n");
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    unsigned control;

    SetUnhandledExceptionFilter(crash_report);
    setvbuf(stdout, NULL, _IONBF, 0);
    /* The game uses 53 bit x87 math. */
    _controlfp_s(&control, _PC_53, _MCW_PC);
    cl_storage_init();
    x87_init();
    /* SR_DEBUGIT=1 shows the editors in the main menu. */
    if (getenv("SR_DEBUGIT") && atoi(getenv("SR_DEBUGIT")) != 0)
        sr_data_debugit = 1;
    /* The smoke driver clears SR_SAMPLE, so SR_NATIVE_SAMPLE does the same for a smoke run. */
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    if ((getenv("SR_SAMPLE") && atoi(getenv("SR_SAMPLE")) != 0) ||
        (getenv("SR_NATIVE_SAMPLE") && atoi(getenv("SR_NATIVE_SAMPLE")) != 0))
        CreateThread(NULL, 0, sampler, NULL, 0, NULL);
#ifdef SR_STORAGE_GUARDS
    CreateThread(NULL, 0, guard_watcher, NULL, 0, NULL);
#endif
#ifdef SR_SMOKE
    if (!client_smoke_start())
        return 2;
    client_main();
    return client_smoke_finish();
#else
    return client_main();
#endif
}
