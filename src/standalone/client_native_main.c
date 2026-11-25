/* The start of the client. The data of the game are C objects, so the start only gives them
 * their first values, sets the math mode and runs the game. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#include "sr_math.h"
#include "sr_runtime.h"

void cl_storage_init(void);
int __cdecl client_main(void);

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
    return client_main();
}
