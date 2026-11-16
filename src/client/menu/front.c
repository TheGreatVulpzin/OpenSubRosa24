/* The front menus: the connect screen, the screen for a machine without OpenGL 2.0, the user
 * list and the host screen. Each runs its own frame loop until the player leaves: it builds
 * the items, reads the input, draws and shows the frame. */
#include "menu/front.h"

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

enum { OPTION_KEY = 1, OPTION_TEXT = 4, OPTION_INT = 6, OPTION_TOGGLE = 10, OPTION_CHOICE = 11 };

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, const unsigned char *, glGetString, (unsigned))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, uint32_t, SDL_GetTicks, (void))

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
