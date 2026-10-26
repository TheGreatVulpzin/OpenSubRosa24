/* The settings of the game: the defaults and the keys of config.txt.
 *
 * Each key is a line "name=value" in config.txt. A key that is not in the file keeps its
 * default. The client window settings go to Config, the others to Option.
 */
#include "options.h"

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define g_option sr_data_option
#define g_config sr_data_config
#define g_sysinfo sr_data_sysinfo
#define g_parser sr_data_parser

/* Sets the value from the number after the key. Every key search starts at the top of the text. */
static void optionreadwriteint(int *value, const char *key)
{
    if (findstring(key))
        *value = getint();
    g_parser.pos = 0;
}

/* Sets the string from the rest of the line after the key. */
static void optionreadwritestring(char *value, const char *key, int max)
{
    if (findstring(key))
        getstring(value, max);
    g_parser.pos = 0;
}

/* Sets the defaults and reads config.txt from the settings folder. A non-zero save only
 * changes to the settings folder and back. */
void loadsaveoptions(int save)
{
    Option *o = &g_option;
    Config *c = &g_config;

    os_chdir(g_sysinfo.config_dir);
    if (save) {
        os_chdir(g_sysinfo.start_dir);
        return;
    }
    c->screenwidth = 0x400;
    c->screenheight = 0x280;
    c->colordepth = 0x20;
    c->depthbits = 0x18;
    c->stencilbits = 8;
    c->fullscreen = 0;
    c->sound = 1;
    c->unused_1c = 1;
    c->joystick = 1;
    o->port = 0x6bc0;
    o->bandwidth = 100000;
    o->soundvolume = 0x50;
    o->musicvolume = 0x50;
    o->roundtime = 7;
    o->maxnumofplayers = 0x10;
    os_srand((uint32_t)os_time64(NULL));
    sprintf(o->name, "Agent %d", os_rand() & 0x1ff);
    memcpy(o->servername, "Sub Rosa Server", 16);
    o->gametype = 3;
    os_srand((uint32_t)os_time64(NULL));
    sprintf(o->password, "%d", os_rand());
    o->unused_d0 = 2;
    o->unused_d4 = 2;
    o->voicemin = 0x800;
    o->voicechat = 0;
    o->voiceholdtotalk = 1;
    o->voiceboost = 100;
    o->help = 1;
    o->occlusion = 1;

    loadtextfile("config.txt");
    optionreadwriteint(&c->screenwidth, "screenwidth=");
    optionreadwriteint(&c->screenheight, "screenheight=");
    optionreadwriteint(&c->colordepth, "colordepth=");
    optionreadwriteint(&c->depthbits, "depthbits=");
    optionreadwriteint(&c->stencilbits, "stencilbits=");
    optionreadwriteint(&c->fullscreen, "fullscreen=");
    optionreadwriteint(&c->joystick, "joystick=");
    optionreadwriteint(&c->sound, "sound=");
    optionreadwriteint(&o->soundvolume, "soundvolume=");
    optionreadwriteint(&o->musicvolume, "musicvolume=");
    optionreadwriteint(&o->port, "port=");
    optionreadwriteint(&o->bandwidth, "servermaxbytespersecond=");
    optionreadwritestring(o->name, "name=", 0x1e);
    optionreadwritestring(o->servername, "servername=", 0x32);
    optionreadwritestring(o->password, "serverpassword=", 0x32);
    optionreadwriteint(&o->gametype, "gametype=");
    optionreadwriteint(&o->invertmouse, "invertmouse=");
    optionreadwriteint(&o->maxnumofplayers, "maxnumofplayers=");
    optionreadwriteint(&o->roundtime, "roundtime=");
    optionreadwriteint(&o->setcpuaffinity, "setcpuaffinity=");
    optionreadwriteint(&o->voicechat, "voicechat=");
    optionreadwriteint(&o->voiceholdtotalk, "voiceholdtotalk=");
    optionreadwriteint(&o->voicekey, "voicekey=");
    optionreadwriteint(&o->voicemb, "voicemb=");
    optionreadwriteint(&o->voicemin, "voicemin=");
    optionreadwriteint(&o->voiceboost, "voiceboost=");
    optionreadwriteint(&o->help, "help=");
    optionreadwriteint(&o->occlusion, "occlusion=");

    /* The scale keeps its value when it is a NaN. */
    o->voiceboost_scale = (float)(o->voiceboost / 100.0);
    if (o->voiceboost_scale < 0.125)
        o->voiceboost_scale = 0.125f;
    else if (o->voiceboost_scale > 4.0)
        o->voiceboost_scale = 4.0f;
    os_chdir(g_sysinfo.start_dir);
}
