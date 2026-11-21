/* Exercise menus and editors with the window and graphics driver active.
 * The worker sends SDL events. Poll wrappers supply the same input state.
 * Frame snapshots let the worker read menu items without a concurrent write.
 * No game record receives a test write.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_smoke.h"
#include "../../instrumentation/client/runtime/storage_adapter.h"

enum {
    MENU_WIDTH = 1024, MENU_HEIGHT = 640, MAX_ITEMS = 256, KEY_COUNT = 512,
    EDITOR_FRAMES = 120, INPUT_FRAMES = 2, MENU_MS = 5000,
    START_MS = 180000, STEP_MS = 120000, FRAME_MS = 180000,
    EVENT_QUIT = 0x100, EVENT_KEY_DOWN = 0x300, EVENT_KEY_UP = 0x301,
    EVENT_MOTION = 0x400, EVENT_BUTTON_DOWN = 0x401, EVENT_BUTTON_UP = 0x402,
    KEY_A = 4, KEY_E = 8, KEY_ESCAPE = 41, KEY_F1 = 58,
    LOOP_NONE = 0, LOOP_GAME, LOOP_BUILDING, LOOP_LEVEL, LOOP_MODEL
};

/* SDL2 events occupy 56 bytes on the 32 bit client. */
typedef union SmokeEvent {
    uint32_t type;
    struct { uint32_t type, time, window, which, state; int x, y, dx, dy; } motion;
    struct { uint32_t type, time, window, which; uint8_t button, state, clicks, pad; int x, y; } button;
    struct { uint32_t type, time, window; uint8_t state, repeat, pad[2]; int scan, key; uint16_t mod; } key;
    uint8_t bytes[56];
} SmokeEvent;

typedef struct SmokeItem {
    char text[64];
    float x, y, width, height;
    int disabled, hidden, key;
} SmokeItem;

typedef struct SmokeFrame {
    unsigned number;
    int count, width, height, loop;
    void *window;
    SmokeItem items[MAX_ITEMS];
} SmokeFrame;

static const char *scenario;
static HANDLE worker, changed, stop;
static CRITICAL_SECTION lock;
static SmokeFrame frame;
static volatile LONG mouse_x, mouse_y, buttons, scan_key, active_loop, result;
static void (__cdecl *swap_window)(void *);
static unsigned (__cdecl *mouse_state)(int *, int *);
static const uint8_t *(__cdecl *keyboard_state)(int *);
static int (__cdecl *push_event)(SmokeEvent *);
static void (__cdecl *warp_mouse)(void *, int, int);
static uint32_t (__cdecl *window_id)(void *);
static uint8_t keys[KEY_COUNT];

static SmokeFrame snapshot(void)
{
    SmokeFrame copy;
    EnterCriticalSection(&lock);
    copy = frame;
    LeaveCriticalSection(&lock);
    return copy;
}

/* Copy only fields that the worker needs, after the frame reaches the screen. */
static void __cdecl smoke_swap(void *window)
{
    int i, count;
    swap_window(window);
    EnterCriticalSection(&lock);
    frame.number++;
    frame.window = window;
    frame.width = cl_data_render_width;
    frame.height = cl_data_render_height;
    frame.loop = (int)InterlockedCompareExchange(&active_loop, 0, 0);
    count = cl_data_menu_item_count;
    frame.count = count >= 0 && count <= MAX_ITEMS ? count : 0;
    for (i = 0; i < frame.count; i++) {
        const MenuItem *item = &cl_data_menu_items[i];
        SmokeItem *copy = &frame.items[i];
        memcpy(copy->text, item->text, sizeof copy->text);
        copy->text[sizeof copy->text - 1] = 0;
        copy->x = item->x;
        copy->y = item->y;
        copy->width = item->width;
        copy->height = item->height;
        copy->disabled = item->disabled;
        copy->hidden = item->hidden;
        copy->key = item->key;
    }
    LeaveCriticalSection(&lock);
    SetEvent(changed);
}

/* SDL_PushEvent does not update the state returned by SDL polling calls. */
static unsigned __cdecl smoke_mouse(int *x, int *y)
{
    mouse_state(x, y);
    if (x) *x = (int)InterlockedCompareExchange(&mouse_x, 0, 0);
    if (y) *y = (int)InterlockedCompareExchange(&mouse_y, 0, 0);
    return (unsigned)InterlockedCompareExchange(&buttons, 0, 0);
}

static const uint8_t *__cdecl smoke_keyboard(int *count)
{
    LONG key = InterlockedCompareExchange(&scan_key, 0, 0);
    keyboard_state(NULL);
    memset(keys, 0, sizeof keys);
    if (key > 0 && key < KEY_COUNT) keys[key] = 1;
    if (count) *count = KEY_COUNT;
    return keys;
}

/* Keep test settings inside the folder selected by the executor. */
static int __stdcall smoke_documents(void *window, char *path, int folder, int create)
{
    DWORD size;
    (void)window;
    (void)folder;
    (void)create;
    size = GetCurrentDirectoryA(MAX_PATH, path);
    return size > 0 && size < MAX_PATH;
}

void *client_smoke_import(const char *dll, const char *name, void *function)
{
    if (!getenv("SR_SMOKE")) return function;
    if (!_stricmp(dll, "SHELL32.DLL") && !strcmp(name, "SHGetSpecialFolderPathA"))
        return (void *)smoke_documents;
    if (_stricmp(dll, "SDL2.DLL")) return function;
    if (!strcmp(name, "SDL_GL_SwapWindow")) {
        swap_window = (void (__cdecl *)(void *))function;
        return (void *)smoke_swap;
    }
    if (!strcmp(name, "SDL_GetMouseState")) {
        mouse_state = (unsigned (__cdecl *)(int *, int *))function;
        return (void *)smoke_mouse;
    }
    if (!strcmp(name, "SDL_GetKeyboardState")) {
        keyboard_state = (const uint8_t *(__cdecl *)(int *))function;
        return (void *)smoke_keyboard;
    }
    return function;
}

/* Record entry into a frame loop. Menu snapshots also check its visible controls. */
void client_smoke_call(unsigned rva)
{
    if (!scenario) return;
    switch (rva) {
    case 0x7b100: InterlockedExchange(&active_loop, LOOP_GAME); break;
    case 0x58d20: InterlockedExchange(&active_loop, LOOP_BUILDING); break;
    case 0x9c5c0: InterlockedExchange(&active_loop, LOOP_LEVEL); break;
    case 0xc14a0: InterlockedExchange(&active_loop, LOOP_MODEL); break;
    }
}

static int item_index(const SmokeFrame *view, const char *text)
{
    int i;
    for (i = 0; i < view->count; i++)
        if (!strcmp(view->items[i].text, text)) return i;
    return -1;
}

typedef int (*ScreenCheck)(const SmokeFrame *);
static int main_screen(const SmokeFrame *v) { return item_index(v, "Voice Options") >= 0 && item_index(v, "Demo") >= 0; }
static int options_screen(const SmokeFrame *v) { return item_index(v, "Apply") >= 0 && item_index(v, "Fullscreen") >= 0; }
static int voice_screen(const SmokeFrame *v) { return item_index(v, "Back") >= 0 && !options_screen(v) && item_index(v, "Direct IP") < 0 && item_index(v, "Host") < 0; }
static int host_screen(const SmokeFrame *v) { return item_index(v, "Max Players") >= 0 && item_index(v, "Host") >= 0; }
static int join_screen(const SmokeFrame *v) { return item_index(v, "Direct IP") >= 0; }
static int list_screen(const SmokeFrame *v) { return item_index(v, "New Building") >= 0; }
static int game_screen(const SmokeFrame *v) { return v->count > 1 && v->items[1].hidden && v->items[1].key == KEY_ESCAPE; }
static int pause_screen(const SmokeFrame *v) { return item_index(v, "Press E to exit") >= 0; }
static int building_screen(const SmokeFrame *v) { return v->loop == LOOP_BUILDING && item_index(v, "Name") >= 0 && item_index(v, "Exit") == 0; }
static int level_screen(const SmokeFrame *v) { return v->loop == LOOP_LEVEL && item_index(v, "Exit") == 0 && !game_screen(v) && !list_screen(v) && !main_screen(v); }
static int model_screen(const SmokeFrame *v) { return v->loop == LOOP_MODEL && v->count == 1 && item_index(v, "Exit") == 0; }

static int fail(const char *reason)
{
    fprintf(stderr, "SMOKE FAIL %s: %s\n", scenario, reason);
    InterlockedExchange(&result, 2);
    return 0;
}

/* A time limit bounds every wait, including a frame wait after a click. */
static int wait_screen(ScreenCheck check, unsigned frames, DWORD milliseconds, DWORD limit, const char *stage)
{
    DWORD start = GetTickCount(), first_time = 0;
    unsigned first_frame = 0;
    int seen = 0;
    fprintf(stderr, "SMOKE STAGE %s\n", stage);
    while (GetTickCount() - start < limit) {
        SmokeFrame view = snapshot();
        if (view.number && (!check || check(&view))) {
            if (!seen) { first_frame = view.number; first_time = GetTickCount(); seen = 1; }
            if (view.number - first_frame >= frames && GetTickCount() - first_time >= milliseconds) {
                fprintf(stderr, "SMOKE CHECK %s frames=%u ms=%lu\n", stage, view.number - first_frame,
                        (unsigned long)(GetTickCount() - first_time));
                return 1;
            }
        } else {
            seen = 0;
        }
        if (WaitForSingleObject(stop, 0) == WAIT_OBJECT_0) return 0;
        WaitForSingleObject(changed, 20);
    }
    fprintf(stderr, "SMOKE TIMEOUT %s\n", stage);
    return fail(stage);
}

static int push(SmokeEvent *event)
{
    return push_event(event) == 1 || fail("SDL_PushEvent");
}

static int click(const char *text)
{
    SmokeFrame view = snapshot();
    SmokeEvent event = {0};
    int index = item_index(&view, text), x, y;
    const SmokeItem *item;
    if (index < 0 || !view.window || view.width <= 0 || view.height <= 0) return fail(text);
    item = &view.items[index];
    if (item->disabled || item->hidden || !(item->width > 0) || !(item->height > 0)) return fail(text);
    x = (int)((item->x + item->width * 0.5) * view.width / MENU_WIDTH);
    y = (int)((item->y + item->height * 0.5) * view.height / MENU_HEIGHT);
    if (x < 0 || x >= view.width || y < 0 || y >= view.height) return fail("button outside window");
    fprintf(stderr, "SMOKE CLICK %s pixel=%d,%d menu=%.1f,%.1f,%.1f,%.1f\n",
            text, x, y, item->x, item->y, item->width, item->height);
    InterlockedExchange(&mouse_x, x);
    InterlockedExchange(&mouse_y, y);
    warp_mouse(view.window, x, y);
    event.motion.type = EVENT_MOTION;
    event.motion.window = window_id(view.window);
    event.motion.x = x;
    event.motion.y = y;
    if (!push(&event)) return 0;
    if (!wait_screen(NULL, INPUT_FRAMES, 0, FRAME_MS, "mouse position")) return 0;
    memset(&event, 0, sizeof event);
    event.button.type = EVENT_BUTTON_DOWN;
    event.button.window = window_id(view.window);
    event.button.button = 1;
    event.button.state = 1;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    InterlockedExchange(&buttons, 1);
    if (!push(&event)) { InterlockedExchange(&buttons, 0); return 0; }
    index = wait_screen(NULL, INPUT_FRAMES, 0, FRAME_MS, "mouse down");
    InterlockedExchange(&buttons, 0);
    event.button.type = EVENT_BUTTON_UP;
    event.button.state = 0;
    if (!push(&event) || !index) return 0;
    return wait_screen(NULL, INPUT_FRAMES, 0, FRAME_MS, "mouse release");
}

static int key_press(int scan)
{
    SmokeFrame view = snapshot();
    SmokeEvent event = {0};
    int ok;
    event.key.type = EVENT_KEY_DOWN;
    event.key.window = window_id(view.window);
    event.key.state = 1;
    event.key.scan = scan;
    event.key.key = scan == KEY_ESCAPE ? 27 : scan == KEY_A ? 'a' : scan == KEY_E ? 'e' : (1 << 30) | scan;
    InterlockedExchange(&scan_key, scan);
    if (!push(&event)) { InterlockedExchange(&scan_key, 0); return 0; }
    ok = wait_screen(NULL, INPUT_FRAMES, 0, FRAME_MS, "key down");
    InterlockedExchange(&scan_key, 0);
    event.key.type = EVENT_KEY_UP;
    event.key.state = 0;
    if (!push(&event) || !ok) return 0;
    return wait_screen(NULL, INPUT_FRAMES, 0, FRAME_MS, "key release");
}

static int menus(void)
{
    static const struct { const char *button; ScreenCheck screen; } steps[] = {
        {"Options", options_screen}, {"Voice Options", voice_screen}, {"Start", host_screen}, {"Join", join_screen}
    };
    unsigned i;
    if (!wait_screen(main_screen, 30, MENU_MS, STEP_MS, "main menu")) return 0;
    for (i = 0; i < sizeof steps / sizeof steps[0]; i++) {
        if (!click(steps[i].button) || !wait_screen(steps[i].screen, 30, MENU_MS, STEP_MS, steps[i].button) ||
            !click("Back") || !wait_screen(main_screen, INPUT_FRAMES, 0, STEP_MS, "back to main")) return 0;
    }
    return 1;
}

static int leave_game(ScreenCheck destination)
{
    return key_press(KEY_ESCAPE) && wait_screen(pause_screen, INPUT_FRAMES, 0, STEP_MS, "game pause") &&
           key_press(KEY_E) && wait_screen(destination, INPUT_FRAMES, 0, STEP_MS, "leave game");
}

static int buildings(void)
{
    WIN32_FIND_DATAA data;
    HANDLE files = FindFirstFileA("data/building/*.sbb", &data);
    unsigned count = 0;
    int ok = files != INVALID_HANDLE_VALUE;
    if (!ok) return fail("no building files");
    if (!click("Editor") || !wait_screen(list_screen, INPUT_FRAMES, 0, STEP_MS, "building list")) ok = 0;
    while (ok) {
        char *dot = strrchr(data.cFileName, '.');
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && dot) {
            *dot = 0;
            fprintf(stderr, "SMOKE BUILDING %s\n", data.cFileName);
            ok = click(data.cFileName) && wait_screen(game_screen, INPUT_FRAMES, 0, STEP_MS, "building game") &&
                 key_press(KEY_F1) && wait_screen(building_screen, EDITOR_FRAMES, 0, FRAME_MS, "building editor") &&
                 click("Exit") && wait_screen(game_screen, INPUT_FRAMES, 0, STEP_MS, "editor return") &&
                 leave_game(list_screen);
            if (ok) { count++; fprintf(stderr, "SMOKE BUILDING PASS %s frames=%d\n", data.cFileName, EDITOR_FRAMES); }
        }
        if (!FindNextFileA(files, &data)) {
            if (GetLastError() != ERROR_NO_MORE_FILES) ok = fail("building file enumeration");
            break;
        }
    }
    FindClose(files);
    if (!count && ok) ok = fail("empty building list");
    fprintf(stderr, "SMOKE BUILDINGS tested=%u\n", count);
    return ok && click("Exit") && wait_screen(main_screen, INPUT_FRAMES, 0, STEP_MS, "back to main");
}

static int level(void)
{
    return click("Demo") && wait_screen(game_screen, INPUT_FRAMES, 0, STEP_MS, "level game") &&
           key_press(KEY_F1) && wait_screen(level_screen, EDITOR_FRAMES, 0, FRAME_MS, "level editor") &&
           click("Exit") && wait_screen(game_screen, INPUT_FRAMES, 0, STEP_MS, "editor return") &&
           leave_game(main_screen);
}

static int model(void)
{
    /* Debug mode exposes this viewer through A, without a menu button. */
    return key_press(KEY_A) && wait_screen(model_screen, EDITOR_FRAMES, 0, FRAME_MS, "model viewer") &&
           click("Exit") && wait_screen(main_screen, INPUT_FRAMES, 0, STEP_MS, "back to main");
}

static int crash(void)
{
    WIN32_FIND_DATAA data;
    HANDLE files = FindFirstFileA("data/building/*.sbb", &data);
    char *dot;
    if (files == INVALID_HANDLE_VALUE) return fail("no building files");
    FindClose(files);
    dot = strrchr(data.cFileName, '.');
    if (!dot) return fail("building extension");
    *dot = 0;
    fprintf(stderr, "SMOKE BUILDING %s\n", data.cFileName);
    return click("Editor") && wait_screen(list_screen, INPUT_FRAMES, 0, STEP_MS, "building list") &&
           click(data.cFileName) && wait_screen(game_screen, EDITOR_FRAMES, 0, FRAME_MS, "building game");
}

static DWORD WINAPI smoke_worker(void *unused)
{
    HMODULE sdl;
    int ok;
    DWORD start = GetTickCount();
    (void)unused;
    fprintf(stderr, "SMOKE READY %s\n", scenario);
    while (!(sdl = GetModuleHandleA("SDL2.DLL"))) {
        if (WaitForSingleObject(stop, 20) == WAIT_OBJECT_0) return 0;
        if (GetTickCount() - start >= START_MS) { fail("SDL2 load timeout"); return 0; }
    }
    push_event = (int (__cdecl *)(SmokeEvent *))GetProcAddress(sdl, "SDL_PushEvent");
    warp_mouse = (void (__cdecl *)(void *, int, int))GetProcAddress(sdl, "SDL_WarpMouseInWindow");
    window_id = (uint32_t (__cdecl *)(void *))GetProcAddress(sdl, "SDL_GetWindowID");
    if (!push_event || !warp_mouse || !window_id) { fail("SDL2 test exports"); return 0; }
    ok = wait_screen(main_screen, INPUT_FRAMES, 0, START_MS, "startup menu");
    if (ok) {
        if (!strcmp(scenario, "crash")) ok = crash();
        else if (!strcmp(scenario, "menus")) ok = menus();
        else if (!strcmp(scenario, "buildings")) ok = buildings();
        else if (!strcmp(scenario, "level")) ok = level();
        else ok = model();
    }
    InterlockedExchange(&buttons, 0);
    InterlockedExchange(&scan_key, 0);
    if (ok) { InterlockedExchange(&result, 1); fprintf(stderr, "SMOKE PASS %s\n", scenario); }
    fprintf(stderr, "SMOKE STAGE quit\n");
    {
        SmokeEvent event = {0};
        event.type = EVENT_QUIT;
        push(&event);
    }
    return 0;
}

int client_smoke_start(void)
{
    scenario = getenv("SR_SMOKE");
    if (!scenario) return 1;
    if (strcmp(scenario, "crash") && strcmp(scenario, "menus") && strcmp(scenario, "buildings") && strcmp(scenario, "level") && strcmp(scenario, "model"))
        return fail("unknown scenario");
    InitializeCriticalSection(&lock);
    changed = CreateEventA(NULL, FALSE, FALSE, NULL);
    stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!changed || !stop) return fail("event allocation");
    worker = CreateThread(NULL, 0, smoke_worker, NULL, 0, NULL);
    if (!worker) return fail("thread allocation");
    return 1;
}

int client_smoke_finish(void)
{
    LONG status;
    if (!scenario) return 0;
    SetEvent(stop);
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
    CloseHandle(changed);
    CloseHandle(stop);
    DeleteCriticalSection(&lock);
    status = InterlockedCompareExchange(&result, 0, 0);
    if (status != 1) { fprintf(stderr, "SMOKE FAIL %s: incomplete run\n", scenario); return 2; }
    return 0;
}

