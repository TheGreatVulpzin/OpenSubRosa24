/* The front menus: the connect screen, the screen for a machine without OpenGL 2.0, the user
 * list and the host screen. Each runs its own frame loop until the player leaves: it builds
 * the items, reads the input, draws and shows the frame. */
#include "menu/front.h"

#include <stdio.h>
#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "sr_runtime.h"

#define KEY_ESCAPE 0x29
#define KEY_ENTER 0x28
#define KEY_S 0x16
#define KEY_PAGE_UP 0x4b
#define KEY_PAGE_DOWN 0x4e
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_CLEAR_BITS 0x4100 /* color and depth */
#define GL_VENDOR 0x1f00
#define GL_RENDERER 0x1f01
#define GL_VERSION 0x1f02
#define GL_EXTENSIONS 0x1f03

enum { OPTION_KEY = 1, OPTION_TEXT = 4, OPTION_INT = 6, OPTION_TOGGLE = 10, OPTION_CHOICE = 11,
       OPTION_SLIDER = 19 };

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, const unsigned char *, glGetString, (unsigned))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, uint32_t, SDL_GetTicks, (void))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_GL_SetAttribute, (int, int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_GL_GetAttribute, (int, int *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_SetWindowSize, (void *, int, int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_SetWindowFullscreen, (void *, uint32_t))
AUDIO_IMPORT("OPENAL32.DLL", __cdecl, const char *, alGetString, (int))

/* Game functions of other files. */
#define checksystemmessages CL_FN(0x5060d0, void(__cdecl *)(void))
#define checkkeyboard CL_FN(0x4fec50, void(__cdecl *)(void))
#define checkmouse CL_FN(0x4feca0, void(__cdecl *)(void))
#define setuptextdisplay CL_FN(0x5079d0, void(__cdecl *)(void))
#define drawtext CL_FN(0x506b30, void(__cdecl *)(const char *text, float x, float y, float size, uint32_t flags, \
                                                 float red, float green, float blue, float alpha, ...))
#define launchwebpage CL_FN(0x4ac0a0, void(__cdecl *)(const char *url))
#define unpackangle CL_FN(0x468ec0, void(__cdecl *)(float *angle, uint32_t bits))
#define gameloop CL_FN(0x47b100, void(__cdecl *)(void))
#define recvpacket CL_FN(0x4fe420, int(__cdecl *)(void))
#define resetsendpacket CL_FN(0x4fe4f0, void(__cdecl *)(void))
#define sendpacket CL_FN(0x4fe520, void(__cdecl *)(uint32_t address, uint16_t port))
#define cs_pwrite CL_FN(0x4fe5b0, int(__cdecl *)(const void *data, int size, int count))
#define cs_pread CL_FN(0x4fe630, int(__cdecl *)(void *data, int size, int count))
#define pbitwrite CL_FN(0x4fe6c0, int(__cdecl *)(const uint32_t *value, uint32_t bits))
#define pbitread CL_FN(0x4fe800, int(__cdecl *)(uint32_t *value, uint32_t bits))
#define enablesocketserver CL_FN(0x4fe2b0, void(__cdecl *)(int lan))
#define closesocketserver CL_FN(0x4fe3f0, void(__cdecl *)(int lan))
#define loadaccountsserver CL_FN(0x4341d0, void(__cdecl *)(void))
#define saveaccountsclient CL_FN(0x434470, void(__cdecl *)(void))
#define setupserver CL_FN(0x4345d0, void(__cdecl *)(void))
#define endserver CL_FN(0x434010, void(__cdecl *)(void))
#define reactivategame CL_FN(0x47d060, void(__cdecl *)(void))
#define checksimulation CL_FN(0x47d900, int(__cdecl *)(int))
#define set_cpu_affinity CL_FN(0x4faf30, void(__cdecl *)(void)) /* setcpuaffinity */
#define enablesocketclient CL_FN(0x4fe360, void(__cdecl *)(int lan))
#define getlanipaddress CL_FN(0x494b20, void(__cdecl *)(void))
#define getonlineserverlist CL_FN(0x494c00, void(__cdecl *)(void))
#define createclientaccount CL_FN(0x433f20, void(__cdecl *)(int *id_a, int *id_b, int key))

/* Clear the screen to the menu color with the given bits. */
static void clear_menu_screen(unsigned bits)
{
    audio_glClearColor()(cl_data_menu_clear_color[0], cl_data_menu_clear_color[1], cl_data_menu_clear_color[2], 0.0f);
    audio_glClear()(bits);
}

/* Read the input of a frame and check the items. */
static void menu_input(void)
{
    checksystemmessages();
    checkkeyboard();
    checkmouse();
    checkmenuitems();
}

/* Draw the items and the mouse cursor, and show the frame. */
static void menu_show(int cursor)
{
    drawmenuitems();
    drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, cursor, 1.0f, 1.0f, 1.0f, 1.0f);
    audio_SDL_GL_SwapWindow()(cl_data_audio_window);
}

/* The menu loops end when the back item is pressed or the window closes. */
static int menu_open(void)
{
    return cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0;
}

/* --- connectmenu --- */

#define CONNECT_REQUEST 2
#define CONNECT_REPLY 6
#define RESEND_MS 100
#define RESEND_LIMIT 5

/* Send the connect request: the version, the name of the player and its account id. */
static void send_connect_request(void)
{
    char kind = CONNECT_REQUEST;

    resetsendpacket();
    cs_pwrite(&kind, 1, 1);
    pbitwrite((const uint32_t *)&sr_data_currentversion, 8);
    cs_pwrite(sr_data_option.name, 1, 0x20);
    cs_pwrite(&cl_data_client_account_id[0], 4, 1);
    cs_pwrite(&cl_data_client_account_id[1], 4, 1);
    sendpacket(cl_data_server_address, cl_data_server_port);
}

/* Read the packets that came. A reply of kind 6 gives the round number, the game type, the
 * round mode and the two light angles. Return 1 when a reply came. */
static int read_connect_replies(void)
{
    int connected = 0;
    char kind;

    while (recvpacket() > 0) {
        cs_pread(&kind, 1, 1);
        if (kind == CONNECT_REPLY) {
            cs_pread(&sr_data_area_sector.client_game_id, 4, 1);
            pbitread((uint32_t *)&sr_data_game.gametype, 4);
            pbitread((uint32_t *)&sr_data_game.round_mode, 4);
            unpackangle(&cl_data_light_angle_a, 16);
            unpackangle(&cl_data_light_angle_b, 16);
            connected = 1;
        }
    }
    return connected;
}

/* Show "Connecting..." and send the request again each 100 ms, at most 5 times in a frame,
 * until the server answers or the player cancels. Then run the game. */
void __cdecl connectmenu(void)
{
    int connected = 0;
    uint32_t last;

    resetmenuitems();
    last = audio_SDL_GetTicks()();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0 && !connected) {
        int sent;

        clear_menu_screen(GL_COLOR_BUFFER_BIT);
        cl_data_menu_item_count = 0;
        createmenubutton("Cancel", 0.0f, 636.0f, 192.0f, 24.0f, 20.0f, 8, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_KEY, KEY_ESCAPE);
        menu_input();
        setuptextdisplay();
        drawtext("Connecting...", 512.0f, 24.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
        menu_show(16);
        /* After three steps of 100 ms, the clock starts again from now. */
        for (sent = 0; audio_SDL_GetTicks()() - last > RESEND_MS && sent < RESEND_LIMIT;) {
            sent++;
            if (sent < 4)
                last += RESEND_MS;
            else
                last = audio_SDL_GetTicks()();
            send_connect_request();
        }
        connected = read_connect_replies();
    }
    if (connected) {
        clear_menu_screen(GL_COLOR_BUFFER_BIT);
        drawtext("Connected, Wait...", 512.0f, 24.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        gameloop();
    }
    resetmenuitems();
}

/* --- notsupportedmenu --- */

/* The links of the screen: button text, y and address. */
static const struct {
    const char *text;
    float y;
    const char *url;
} driver_links[3] = {
    {"NVidia webpage", 192.0f, "www.nvidia.com"},
    {"ATI/AMD webpage", 216.0f, "www.amd.com"},
    {"Intel webpage", 240.0f, "www.intel.com"},
};

/* The screen for a machine without OpenGL 2.0: the message, the OpenGL strings and links to
 * the driver pages. A link opens its page and closes the screen. The vendor and the renderer
 * lines have the same place. */
void __cdecl notsupportedmenu(void)
{
    const char *vendor = (const char *)audio_glGetString()(GL_VENDOR);
    const char *renderer = (const char *)audio_glGetString()(GL_RENDERER);
    const char *version = (const char *)audio_glGetString()(GL_VERSION);
    int i;

    audio_glGetString()(GL_EXTENSIONS);
    resetmenuitems();
    while (menu_open()) {
        audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
        audio_glClear()(GL_CLEAR_BITS);
        cl_data_menu_item_count = 0;
        createmenubutton("Exit", 0.0f, 0.0f, 128.0f, 20.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_KEY, KEY_ESCAPE);
        for (i = 0; i < 3; i++)
            createmenubutton(driver_links[i].text, 512.0f, driver_links[i].y, 128.0f, 24.0f, 16.0f, 1, 1.0f, 1.0f,
                             1.0f, 1.0f);
        menu_input();
        setuptextdisplay();
        drawtext("OpenGL 2.0 required", 512.0f, 128.0f, 16.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
        drawtext("Make sure you have the latest drivers for your video card", 512.0f, 144.0f, 16.0f, 1, 1.0f, 1.0f,
                 1.0f, 1.0f);
        drawtext("GL_VERSION: /s", 4.0f, 320.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, version);
        drawtext("GL_VENDOR: /s", 4.0f, 336.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, vendor);
        drawtext("GL_RENDERER: /s", 4.0f, 336.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, renderer);
        menu_show(16);
        for (i = 0; i < 3; i++) {
            if (cl_data_menu_items[i + 1].pressed != 0) {
                launchwebpage(driver_links[i].url);
                cl_data_menu_items[i + 1].pressed = 0;
                cl_data_menu_items[0].pressed = 1;
            }
        }
    }
    resetmenuitems();
}

/* --- usermenu --- */

#define USERS_PER_PAGE 32

/* The list of the users of the server, 32 for each page: the name and the cash of each user. */
void __cdecl usermenu(void)
{
    int page = 0;

    resetmenuitems();
    while (menu_open()) {
        float y = 64.0f;
        int i;

        audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
        audio_glClear()(GL_COLOR_BUFFER_BIT);
        cl_data_menu_item_count = 0;
        createmenubutton("Back", 4.0f, 636.0f, 192.0f, 24.0f, 12.0f, 8, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_KEY, KEY_ESCAPE);
        /* A missing page button is an empty item, so the item numbers stay the same. */
        if (page == 0) {
            createmenuitemempty();
        } else {
            createmenubutton("Page Up", 664.0f, 576.0f, 160.0f, 24.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &page, page - 1);
            setmenuitem(OPTION_KEY, KEY_PAGE_UP);
        }
        if ((page + 1) * USERS_PER_PAGE < sr_data_accounts.count) {
            createmenubutton("Page Down", 664.0f, 600.0f, 160.0f, 24.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &page, page + 1);
            setmenuitem(OPTION_KEY, KEY_PAGE_DOWN);
        } else {
            createmenuitemempty();
        }
        menu_input();
        setuptextdisplay();
        drawtext("User Name", 4.0f, 16.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        drawtext("Cash", 320.0f, 16.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        for (i = 0; i < USERS_PER_PAGE; i++) {
            int user = page * USERS_PER_PAGE + i;

            if (user < sr_data_accounts.count) {
                drawtext(sr_data_accounts.records[user].name, 4.0f, y, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
                drawtext("/i", 320.0f, y, 16.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f, sr_data_accounts.records[user].money);
                y = (float)(y + 16.0);
            }
        }
        drawtext("Clients:/i", 512.0f, 16.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, sr_data_accounts.count);
        menu_show(24);
    }
    resetmenuitems();
}

/* --- hostmenu --- */

#define TICKS_PER_MINUTE 3600


/* Build the items of the host screen. The broadcast toggle and the server name show only for
 * a server on the network. */
static void host_items(void)
{
    float y = 320.0f;

    cl_data_menu_item_count = 0;
    createmenubutton("Back", 4.0f, 636.0f, 160.0f, 24.0f, 20.0f, 8, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    createmenubutton("Start", 1020.0f, 636.0f, 160.0f, 24.0f, 20.0f, 10, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_S);
    createmenubutton("Host", 4.0f, y, 128.0f, 32.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TOGGLE, &sr_data_option.net_on, 1);
    y = (float)(y + 32.0);
    if (sr_data_option.net_on != 0) {
        createmenubutton("Broadcast", 4.0f, y, 128.0f, 32.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_TOGGLE, &sr_data_option.master_on, 1);
        createmenubutton("Server Name", 4.0f, (float)(y + 52.0), 160.0f, 24.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_TEXT, sr_data_option.servername, 24);
        setmenuitem(OPTION_KEY, KEY_ENTER);
    }
    createmenubutton("Max Players", 4.0f, 128.0f, 128.0f, 22.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_INT, &sr_data_option.maxnumofplayers, 3);
    createmenubutton("Round Length", 4.0f, 164.0f, 128.0f, 22.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_INT, &sr_data_option.roundtime, 3);
}

/* Keep the round length in 1 to 30 minutes and the players in 2 to 32. */
static void host_limits(void)
{
    if (sr_data_option.roundtime < 1)
        sr_data_option.roundtime = 1;
    if (sr_data_option.roundtime > 30)
        sr_data_option.roundtime = 30;
    if (sr_data_option.maxnumofplayers < 2)
        sr_data_option.maxnumofplayers = 2;
    if (sr_data_option.maxnumofplayers > 32)
        sr_data_option.maxnumofplayers = 32;
}

/* Start a server in this client with the settings, run the game, then stop the server and
 * save the accounts. */
static void host_start(void)
{
    strcpy(sr_data_server.name, sr_data_option.servername);
    sr_data_server.max_players = sr_data_option.maxnumofplayers;
    sr_data_game.wait_time = TICKS_PER_MINUTE;
    sr_data_game.round_time = sr_data_option.roundtime * TICKS_PER_MINUTE;
    sr_data_game.teamkill_rate = 0x4ec;
    if (sr_data_option.net_on != 0)
        enablesocketserver(0);
    loadaccountsserver();
    setupserver();
    sr_data_server.active = 1;
    sr_data_game.client_account_key = sr_data_accounts.key;
    gameloop();
    endserver();
    saveaccountsclient();
    if (sr_data_option.net_on != 0)
        closesocketserver(0);
}

/* The host screen: the server settings and Start. Start runs a game with this client as the
 * server, then the screen closes. The screen clears after the input, so the items of the frame
 * draw over a new screen. */
void __cdecl hostmenu(void)
{
    resetmenuitems();
    while (menu_open()) {
        host_items();
        menu_input();
        clear_menu_screen(GL_CLEAR_BITS);
        setuptextdisplay();
        host_limits();
        menu_show(24);
        if (cl_data_menu_items[1].pressed != 0) {
            host_start();
            cl_data_menu_items[0].pressed = 1;
        }
    }
    resetmenuitems();
}

/* --- optionsmenu --- */

#define MODE_PAGE 8
#define KEY_A 4
#define AL_VENDOR 0xb001
#define AL_VERSION 0xb002
#define AL_RENDERER 0xb003
#define SDL_GL_RED_SIZE 0
#define SDL_GL_GREEN_SIZE 1
#define SDL_GL_BLUE_SIZE 2
#define SDL_GL_ALPHA_SIZE 3

/* The name of input i of joystick device d: 0x200 bytes for each device, 16 for each name. */
#define joystick_name(d, i) (&cl_data_joystick_names[(d) * 0x200 + (i) * 0x10])

/* The names of the joystick inputs: the four axes in plus and minus, 16 buttons and the hat
 * directions. */
static void options_input_names(void)
{
    static const char *const plus[4] = {"+X Axis", "+Y Axis", "+Z Axis", "+W Axis"};
    static const char *const minus[4] = {"-X Axis", "-Y Axis", "-Z Axis", "-W Axis"};
    static const char *const hats[8] = {"Hat 1 Up", "Hat 1 Down", "Hat 1 Left", "Hat 1 Right",
                                        "Hat 2 Up", "Hat 2 Down", "Hat 2 Left", "Hat 2 Right"};
    int i;

    for (i = 0; i < 4; i++)
        strcpy(joystick_name(1, i), plus[i]);
    for (i = 0; i < 4; i++)
        strcpy(joystick_name(2, i), minus[i]);
    for (i = 0; i < 16; i++)
        sprintf(joystick_name(3, i), "Button %d", i + 1);
    for (i = 0; i < 8; i++)
        strcpy(joystick_name(4, i), hats[i]);
}

/* The screen mode that matches the window now, or -1. The last match wins. */
static int options_current_mode(void)
{
    int i, mode = -1;

    for (i = 0; i < cl_data_video_mode_count; i++)
        if (cl_data_render_width == cl_data_video_modes[i].width &&
            cl_data_render_height == cl_data_video_modes[i].height &&
            cl_data_screen_bits == cl_data_video_modes[i].bits)
            mode = i;
    return mode;
}

/* Make the items of one frame. The mode list shows a page of eight modes. Only modes of 640
 * pixels or more and 32 bits show. The test of a mode reads the record before the test of the
 * count, so the last page reads records past the list. */
static void options_items(int *fullscreen, int *page, int *mode)
{
    float y = 124.0f;
    int i;

    cl_data_menu_item_count = 0;
    createmenubutton("Back", 4.0f, 4.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    createmenubutton("Apply", 640.0f, 4.0f, 160.0f, 20.0f, 16.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_A);
    createmenubutton("Fullscreen", 4.0f, 64.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TOGGLE, fullscreen, 1);
    if (*page > 0) {
        createmenubutton("Up", 164.0f, y, 80.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, page, *page - 1);
    }
    if (*page * MODE_PAGE + MODE_PAGE < cl_data_video_mode_count) {
        createmenubutton("Down", 164.0f, 264.0f, 80.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, page, *page + 1);
    }
    for (i = *page * MODE_PAGE; i < *page * MODE_PAGE + MODE_PAGE; i++) {
        const VideoMode *m = &cl_data_video_modes[i];

        if (m->width > 0x27f && m->bits == 32 && i < cl_data_video_mode_count) {
            char text[64];

            sprintf(text, "%dx%d", m->width, m->height);
            createmenubutton(text, 4.0f, y, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, mode, i);
            y = (float)(y + 20.0);
        }
    }
    createmenubutton("Port", 512.0f, 240.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_INT, &sr_data_option.port, 8);
    createmenubutton("Bandwidth", 512.0f, 280.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_INT, &sr_data_option.bandwidth, 8);
    createmenubutton("Server Name", 512.0f, 320.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TEXT, sr_data_option.servername, 0x28);
    createmenubutton("Invert Mouse", 512.0f, 360.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TOGGLE, &sr_data_option.invertmouse, 1);
    createmenubutton("Sound Volume", 320.0f, 240.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_SLIDER, &sr_data_option.soundvolume, 0, 100, 10);
    createmenubutton("Music Volume", 320.0f, 280.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_SLIDER, &sr_data_option.musicvolume, 0, 100, 10);
    createmenubutton("Set CPU affinity", 320.0f, 340.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TOGGLE, &sr_data_option.setcpuaffinity, 1);
    createmenubutton("Ambient Occlusion", 320.0f, 360.0f, 160.0f, 20.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TOGGLE, &sr_data_option.occlusion, 1);
}

/* The resolution header and the driver texts of OpenGL and OpenAL. */
static void options_texts(const char *const gl[3], const char *const al[3])
{
    drawtext("Resolution", 16.0f, 96.0f, 12.0f, 0x20, 0.75f, 0.75f, 0.75f, 1.0f);
    drawtext("GL_VERSION: /s", 320.0f, 32.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, gl[2]);
    drawtext("GL_VENDOR: /s", 320.0f, 48.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, gl[0]);
    drawtext("GL_RENDERER: /s", 320.0f, 64.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, gl[1]);
    drawtext("AL_VERSION: /s", 320.0f, 96.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, al[1]);
    drawtext("AL_VENDOR: /s", 320.0f, 112.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, al[0]);
    drawtext("AL_RENDERER: /s", 320.0f, 128.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, al[2]);
    drawtext("OpenAL sources: /i", 320.0f, 144.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, cl_data_audio_source_limit);
}

/* Keep the chosen mode in the settings. The window changes now only for a new mode while the
 * fullscreen state stays off: the start mode, a change of the fullscreen state or a fullscreen
 * window only go to the settings. 16 bits use 5 6 5 bits of color and 1 of alpha, 32 bits use 8
 * of each. When the window gets other than 8 red bits, the depth is 16. */
static void options_apply(int mode, int start_mode, int fullscreen)
{
    int red;

    sr_data_config.fullscreen = fullscreen;
    if (mode == -1)
        return;
    sr_data_config.screenwidth = cl_data_video_modes[mode].width;
    sr_data_config.screenheight = cl_data_video_modes[mode].height;
    if (start_mode == mode || cl_data_screen_fullscreen != fullscreen || fullscreen == 1)
        return;
    cl_data_render_width = cl_data_video_modes[mode].width;
    cl_data_render_height = cl_data_video_modes[mode].height;
    cl_data_screen_fullscreen = fullscreen;
    cl_data_screen_bits = cl_data_video_modes[mode].bits;
    if (cl_data_screen_bits == 16) {
        audio_SDL_GL_SetAttribute()(SDL_GL_RED_SIZE, 5);
        audio_SDL_GL_SetAttribute()(SDL_GL_GREEN_SIZE, 6);
        audio_SDL_GL_SetAttribute()(SDL_GL_BLUE_SIZE, 5);
        audio_SDL_GL_SetAttribute()(SDL_GL_ALPHA_SIZE, 1);
    }
    if (cl_data_screen_bits == 32) {
        audio_SDL_GL_SetAttribute()(SDL_GL_RED_SIZE, 8);
        audio_SDL_GL_SetAttribute()(SDL_GL_GREEN_SIZE, 8);
        audio_SDL_GL_SetAttribute()(SDL_GL_BLUE_SIZE, 8);
        audio_SDL_GL_SetAttribute()(SDL_GL_ALPHA_SIZE, 8);
    }
    audio_SDL_SetWindowSize()(cl_data_audio_window, cl_data_render_width, cl_data_render_height);
    audio_SDL_SetWindowFullscreen()(cl_data_audio_window, cl_data_screen_fullscreen == 0 ? 0 : 1);
    audio_SDL_GL_GetAttribute()(SDL_GL_RED_SIZE, &red);
    if (red != 8)
        cl_data_screen_bits = 16;
    reactivategame();
}

/* The options menu: the screen mode, the fullscreen option, the network and sound options and
 * the driver texts. Back leaves it. Apply also keeps the options and changes the window. */
void __cdecl optionsmenu(void)
{
    const char *gl[3], *al[3];
    int mode, start_mode, fullscreen, page = 0;

    gl[0] = (const char *)audio_glGetString()(GL_VENDOR);
    gl[1] = (const char *)audio_glGetString()(GL_RENDERER);
    gl[2] = (const char *)audio_glGetString()(GL_VERSION);
    audio_glGetString()(GL_EXTENSIONS);
    al[0] = audio_alGetString()(AL_VENDOR);
    al[2] = audio_alGetString()(AL_RENDERER);
    al[1] = audio_alGetString()(AL_VERSION);
    mode = options_current_mode();
    start_mode = mode;
    fullscreen = sr_data_config.fullscreen;
    options_input_names();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_menu_items[1].pressed == 0 && cl_data_audio_menu_back == 0) {
        options_items(&fullscreen, &page, &mode);
        menu_input();
        audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
        audio_glClear()(GL_CLEAR_BITS);
        setuptextdisplay();
        drawmenuitems();
        options_texts(gl, al);
        drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        if (cl_data_audio_simulation != 0)
            checksimulation(0);
    }
    if (sr_data_option.setcpuaffinity != 0)
        set_cpu_affinity();
    if (cl_data_menu_items[1].pressed != 0)
        options_apply(mode, start_mode, fullscreen);
    resetmenuitems();
}

/* --- joinmenu --- */

#define JOIN_ROWS 16
#define PING_MS 10
#define PING_PAUSE 10 /* steps of PING_MS after the end of the address list */
#define PACKET_PING 0
#define PACKET_PONG 1
#define PACKET_MASTER_LIST '!'
#define MASTER_LIMIT 0x2000
#define DIRECT_IP_SIZE 0x18
#define KEY_J 0x0d
#define WSOCK_INET_ADDR 10
#define OPTION_DISABLED 20
#define OPTION_LABEL 21
#define VERSION_SHORT 0x50

/* The two marks of the version column. They are empty texts at fixed addresses. */
#define VERSION_MARK_OLD ((const char *)(sr_base + 0x127eb3u))
#define VERSION_MARK_NEW ((const char *)(sr_base + 0x127eb2u))

/* Swap the bytes between network order and host order. */
static uint32_t swap32(uint32_t value)
{
    return value >> 24 | (value >> 8 & 0xff00u) | (value << 8 & 0xff0000u) | value << 24;
}

static uint16_t swap16(uint16_t value)
{
    return (uint16_t)(value >> 8 | value << 8);
}

/* The sender of the last packet: its address and port in network order. */
#define recv_address (*(const uint32_t *)&sr_data_bsdsocket.addr[4])
#define recv_port (*(const uint16_t *)&sr_data_bsdsocket.addr[2])

/* The y of row i of a list column. */
static float row_y(int i, float column_y)
{
    return (float)((float)i * 32.0 + ((double)column_y + 28.0));
}

/* Build the items: back, join (empty when no server is chosen), the page buttons, one row for
 * each server of the page and the Direct IP field. A row of another version is disabled. */
static void join_items(int *page, int *chosen, char *direct_ip)
{
    const float *col = cl_data_join_columns;
    int i;

    cl_data_menu_item_count = 0;
    createmenubutton("Back", 4.0f, 636.0f, 192.0f, 24.0f, 12.0f, 8, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    if (*chosen == -1) {
        createmenuitemempty();
    } else {
        createmenubutton("Join", 1020.0f, 636.0f, 192.0f, 24.0f, 12.0f, 10, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_KEY, KEY_J);
    }
    if (*page == 0) {
        createmenuitemempty();
    } else {
        createmenubutton("Page Up", 664.0f, 576.0f, 160.0f, 24.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, page, *page - 1);
    }
    if ((*page + 1) * JOIN_ROWS < cl_data_join_server_count) {
        createmenubutton("Page Down", 664.0f, 600.0f, 160.0f, 24.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, page, *page + 1);
    } else {
        createmenuitemempty();
    }
    for (i = 0; i < JOIN_ROWS; i++) {
        int server = *page * JOIN_ROWS + i;

        if (server >= cl_data_join_server_count)
            continue;
        createmenubutton(cl_data_join_servers[server].name, (float)(col[0] - 4.0),
                         (float)((float)i * 32.0 + ((double)col[1] + 24.0)), 960.0f, 32.0f, 20.0f, 0, 1.0f, 1.0f,
                         1.0f, 1.0f);
        /* The label takes x, y and the size as doubles. The last word is not read. */
        setmenuitem(OPTION_LABEL, (double)col[0], (double)i * 32.0 + ((double)col[1] + 28.0), 12.0, 0x40);
        setmenuitem(OPTION_CHOICE, chosen, server);
        if (cl_data_join_servers[server].version != sr_data_currentversion)
            setmenuitem(OPTION_DISABLED);
    }
    createmenubutton("Direct IP", 4.0f, 512.0f, 192.0f, 16.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TEXT, direct_ip, DIRECT_IP_SIZE);
    setmenuitem(OPTION_KEY, KEY_ENTER);
}

/* Add the typed address to the address list with the port of the options, when the player
 * does not type. */
static void join_add_direct_ip(char *direct_ip)
{
    static uint32_t(__stdcall * inet_address)(const char *);
    JoinAddress *a;

    if (cl_data_audio_input_blocked != 0 || direct_ip[0] == '\0')
        return;
    if (!inet_address)
        *(void **)&inet_address = sr_import_ordinal("WSOCK32.DLL", WSOCK_INET_ADDR);
    a = &cl_data_join_addresses[cl_data_join_address_count];
    a->address = swap32(inet_address(direct_ip));
    a->port = (uint16_t)sr_data_option.port;
    cl_data_join_address_count++;
    direct_ip[0] = '\0';
}

/* Draw one row of the server list: the ping, the players, the game type of a server of this
 * version, the version mark and the version. The version is white for this version and red
 * for another one. With debugit, the row also shows the address of server i + page * 8. */
static void join_row(int page, int i)
{
    const float *col = cl_data_join_columns;
    const JoinServer *s = &cl_data_join_servers[page * JOIN_ROWS + i];
    float size = 20.0f, green_blue;
    char mark[64];

    drawtext("/i", col[4], row_y(i, col[5]), size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f, s->ping);
    drawtext("/i///i", col[2], row_y(i, col[3]), size, 2, 1.0f, 1.0f, 1.0f, 1.0f, s->players, s->max_players);
    if (cl_data_audio_debug != 0) {
        const JoinServer *d = &cl_data_join_servers[i + page * 8];
        uint32_t a = d->address;

        drawtext("/i./i./i./i:/i", 320.0f, (float)((float)i * 32.0 + 52.0), 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f,
                 a >> 24, a >> 16 & 0xff, a >> 8 & 0xff, a & 0xff, (uint32_t)d->port);
    }
    if (s->version == sr_data_currentversion) {
        if (s->game_type == 3)
            drawtext("Round", col[8], row_y(i, col[9]), size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
        if (s->game_type == 4)
            drawtext("World", col[8], row_y(i, col[9]), size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    strcpy(mark, s->version < VERSION_SHORT ? VERSION_MARK_OLD : VERSION_MARK_NEW);
    drawtext(mark, col[8], row_y(i, col[9]), size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    green_blue = s->version == sr_data_currentversion ? 1.0f : 0.0f;
    drawtext("Alpha /i", col[6], row_y(i, col[7]), size, 0x22, 1.0f, green_blue, green_blue, 1.0f, s->version);
}

/* Draw the server list: the frame, the headers, the rows of the page and the state of the
 * search. */
static void join_texts(int page, int master_done)
{
    const float *col = cl_data_join_columns;
    float size = 20.0f;
    uint32_t master = sr_data_masterserver.ip;
    int i;

    setuptextdisplay();
    drawbitmap(-1, (float)(col[0] - 4.0), (float)(col[1] + 24.0), 960.0f, 512.0f, 0, 0.0f, 0.0f, 0.0f, 0.75f);
    drawtext("Server Name", col[0], col[1], 20.0f, 0x20, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("Ping", col[4], col[5], size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("Players", col[2], col[3], size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("Game Type", col[8], col[9], size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("Version", col[6], col[7], size, 0x22, 1.0f, 1.0f, 1.0f, 1.0f);
    if (cl_data_audio_debug != 0) {
        cl_data_join_debug[0] = (int)cl_data_join_addresses[0].address;
        cl_data_join_debug[1] = (int)cl_data_join_addresses[1].address;
        cl_data_join_debug[2] = cl_data_join_address_count;
    }
    for (i = 0; i < JOIN_ROWS; i++)
        if (page * JOIN_ROWS + i < cl_data_join_server_count)
            join_row(page, i);
    if (cl_data_audio_debug != 0)
        drawtext("/i", 16.0f, 400.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, cl_data_debug_counters[0]);
    if (cl_data_audio_debug != 0)
        drawtext("/i./i./i./i:/i", 320.0f, 560.0f, 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f, master >> 24, master >> 16 & 0xff,
                 master >> 8 & 0xff, master & 0xff, (uint32_t)(uint16_t)sr_data_masterserver.port);
    if (!master_done) {
        if (sr_data_masterserver.ip == 0)
            drawtext("Unable to connect", 16.0f, 560.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        else
            drawtext("Connecting...", 16.0f, 560.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        drawtext("Connected: /i servers found", 16.0f, 560.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f,
                 cl_data_join_server_count);
        drawtext("/i", 16.0f, 576.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f,
                 cl_data_join_address_count - cl_data_join_master_first);
    }
}

/* Send one ping each 10 ms to the next address of the list. The counter runs 10 steps past the
 * end of the list before it starts again. While the master list has not come, ask for it
 * again. The ping holds the send time. */
static void join_send_ping(uint32_t *last, int *next, int master_done)
{
    if (audio_SDL_GetTicks()() - *last <= PING_MS)
        return;
    *last = audio_SDL_GetTicks()();
    if (cl_data_join_address_count > 0) {
        if (*next < cl_data_join_address_count) {
            char kind = PACKET_PING;

            resetsendpacket();
            cs_pwrite(&kind, 1, 1);
            pbitwrite((const uint32_t *)&sr_data_currentversion, 8);
            cs_pwrite(last, 4, 1);
            sendpacket(cl_data_join_addresses[*next].address, cl_data_join_addresses[*next].port);
        }
        (*next)++;
        if (*next >= cl_data_join_address_count + PING_PAUSE)
            *next = 0;
    }
    if (!master_done)
        getonlineserverlist();
}

/* Read the state of a server from a pong: version, ping, game type, players and max players.
 * The pong gives back the send time, so the ping is now minus that time. */
static void read_server_state(JoinServer *s)
{
    pbitread((uint32_t *)&s->version, 8);
    cs_pread(&s->ping, 4, 1);
    s->ping = (int)(audio_SDL_GetTicks()() - (uint32_t)s->ping);
    pbitread((uint32_t *)&s->game_type, 4);
    pbitread((uint32_t *)&s->players, 8);
    pbitread((uint32_t *)&s->max_players, 8);
}

/* A pong updates the server of the sender address. A pong from a new sender adds a server
 * with its name and account key. The list has no size limit. */
static void join_read_pong(void)
{
    JoinServer *s;
    int i;

    for (i = 0; i < cl_data_join_server_count; i++) {
        if (cl_data_join_servers[i].address == swap32(recv_address) &&
            cl_data_join_servers[i].port == swap16(recv_port)) {
            read_server_state(&cl_data_join_servers[i]);
            return;
        }
    }
    s = &cl_data_join_servers[cl_data_join_server_count];
    s->address = swap32(recv_address);
    s->port = swap16(recv_port);
    read_server_state(s);
    cs_pread(s->name, 1, 0x20);
    cs_pread(&s->account_key, 4, 1);
    cl_data_join_server_count++;
}

/* Read the packets that came. The master list replaces the addresses from the first master
 * address. A list of 0x2000 addresses or more adds nothing. Return 1 when the master list
 * came. */
static int join_read_packets(void)
{
    int master = 0;
    char kind;

    while (recvpacket() > 0) {
        cs_pread(&kind, 1, 1);
        if (kind == PACKET_PONG)
            join_read_pong();
        if (kind == PACKET_MASTER_LIST) {
            int count, i;

            cl_data_join_address_count = cl_data_join_master_first;
            master = 1;
            cs_pread(&count, 4, 1);
            if (count < MASTER_LIMIT)
                for (i = 0; i < count; i++) {
                    cs_pread(&cl_data_join_addresses[cl_data_join_address_count].address, 4, 1);
                    cs_pread(&cl_data_join_addresses[cl_data_join_address_count].port, 2, 1);
                    cl_data_join_address_count++;
                }
            cl_data_debug_counters[0] = count;
        }
    }
    return master;
}

/* The server list: ask the master server and the known addresses, show the answers and join
 * the server that the player chooses. */
void __cdecl joinmenu(void)
{
    int master_done = 0, next = 0, chosen = -1, page = 0;
    char direct_ip[DIRECT_IP_SIZE + 8];
    uint32_t last;

    enablesocketclient(0);
    sr_data_server.active = 0;
    setupserver();
    memset(&sr_data_area_sector, 0, 0x30);
    sr_data_numofconnections = 0;
    cl_data_join_server_count = 0;
    cl_data_join_address_count = 0;
    cl_data_join_master_first = 0;
    getlanipaddress();
    getonlineserverlist();
    direct_ip[0] = '\0';
    cl_data_join_columns[0] = 24.0f;
    cl_data_join_columns[1] = 24.0f;
    cl_data_join_columns[4] = 480.0f;
    cl_data_join_columns[5] = 24.0f;
    cl_data_join_columns[2] = 560.0f;
    cl_data_join_columns[3] = 24.0f;
    cl_data_join_columns[8] = 672.0f;
    cl_data_join_columns[9] = 24.0f;
    cl_data_join_columns[6] = 784.0f;
    cl_data_join_columns[7] = 24.0f;
    last = audio_SDL_GetTicks()();
    resetmenuitems();
    for (;;) {
        if (!menu_open()) {
            closesocketserver(0);
            saveaccountsclient();
            resetmenuitems();
            return;
        }
        clear_menu_screen(GL_COLOR_BUFFER_BIT);
        join_items(&page, &chosen, direct_ip);
        menu_input();
        join_add_direct_ip(direct_ip);
        join_texts(page, master_done);
        menu_show(24);
        join_send_ping(&last, &next, master_done);
        if (join_read_packets())
            master_done = 1;
        if (chosen != -1) {
            cl_data_server_address = cl_data_join_servers[chosen].address;
            cl_data_server_port = cl_data_join_servers[chosen].port;
            sr_data_game.client_account_key = cl_data_join_servers[chosen].account_key;
            createclientaccount(&cl_data_client_account_id[0], &cl_data_client_account_id[1],
                                sr_data_game.client_account_key);
            connectmenu();
            cl_data_menu_items[0].pressed = 1;
        }
    }
}
