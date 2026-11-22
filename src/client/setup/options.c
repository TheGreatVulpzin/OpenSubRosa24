/* Client settings use the window and voice controls of the local player.
 * Loading sets defaults before each key search. Saving writes the current settings.
 * The voice key defaults to V. The mouse binding has no default button.
 */
#include "options.h"
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>

enum {
    DEFAULT_WIDTH = 1024, DEFAULT_HEIGHT = 640, COLOR_BITS = 32,
    DEPTH_BITS = 24, STENCIL_BITS = 8, SERVER_PORT = 27584,
    SERVER_BANDWIDTH = 100000, SOUND_VOLUME = 80, ROUND_MINUTES = 7,
    SERVER_PLAYERS = 16, AGENT_MASK = 511, NETWORK_MODE = 2,
    VOICE_THRESHOLD = 2048, VOICE_KEY_V = 25, VOICE_GAIN_PERCENT = 100,
    PLAYER_NAME_LENGTH = 30, SERVER_TEXT_LENGTH = 50
};
#define VOICE_GAIN_MIN 0.125f
#define VOICE_GAIN_MAX 4.0f

/* Read or write one option. Each read starts at the first line. */
void __cdecl loadsaveoptions(int save)
{
    OptionClient *option = &sr_data_option;
    Config *config = &sr_data_config;
    void *file = NULL;
    os_chdir(sr_data_sysinfo.config_dir);
    if (!save) {
        config->screenwidth = DEFAULT_WIDTH;
        config->screenheight = DEFAULT_HEIGHT;
        config->colordepth = COLOR_BITS;
        config->depthbits = DEPTH_BITS;
        config->stencilbits = STENCIL_BITS;
        config->fullscreen = 0;
        config->sound = 1;
        config->unused_1c = 1;
        config->joystick = 1;
        option->port = SERVER_PORT;
        option->bandwidth = SERVER_BANDWIDTH;
        option->soundvolume = SOUND_VOLUME;
        option->musicvolume = SOUND_VOLUME;
        option->roundtime = ROUND_MINUTES;
        option->maxnumofplayers = SERVER_PLAYERS;
        os_srand((uint32_t)os_time64(NULL));
        sprintf(option->name, "Agent %d", os_rand() & AGENT_MASK);
        strcpy(option->servername, "Sub Rosa Server");
        os_srand((uint32_t)os_time64(NULL));
        sprintf(option->password, "%d", os_rand());
        option->unused_d0 = NETWORK_MODE;
        option->unused_d4 = NETWORK_MODE;
        option->voicemin = VOICE_THRESHOLD;
        option->voicechat = 0;
        option->voiceholdtotalk = 1;
        option->voicekey = VOICE_KEY_V;
        option->voiceboost = VOICE_GAIN_PERCENT;
        option->help = 1;
        option->occlusion = 1;
        loadtextfile("config.txt");
    } else {
        config->colordepth = cl_data_screen_bits;
        config->depthbits = cl_data_render_depth_bits;
        config->stencilbits = cl_data_render_stencil_bits;
        file = os_fopen("config.txt", "wb");
        sr_parser_file = file;
        if (!file) {
            os_chdir(sr_data_sysinfo.start_dir);
            return;
        }
    }
#define INT_SETTING(record, field, key) option_int(&(record)->field, key "=", save, file)
#define TEXT_SETTING(field, key, length) option_string(option->field, key "=", length, save, file)
    INT_SETTING(config, screenwidth, "screenwidth");
    INT_SETTING(config, screenheight, "screenheight");
    INT_SETTING(config, colordepth, "colordepth");
    INT_SETTING(config, depthbits, "depthbits");
    INT_SETTING(config, stencilbits, "stencilbits");
    INT_SETTING(config, fullscreen, "fullscreen");
    INT_SETTING(config, joystick, "joystick");
    INT_SETTING(config, sound, "sound");
    INT_SETTING(option, soundvolume, "soundvolume");
    INT_SETTING(option, musicvolume, "musicvolume");
    INT_SETTING(option, port, "port");
    INT_SETTING(option, bandwidth, "servermaxbytespersecond");
    TEXT_SETTING(name, "name", PLAYER_NAME_LENGTH);
    TEXT_SETTING(servername, "servername", SERVER_TEXT_LENGTH);
    TEXT_SETTING(password, "serverpassword", SERVER_TEXT_LENGTH);
    INT_SETTING(option, invertmouse, "invertmouse");
    INT_SETTING(option, maxnumofplayers, "maxnumofplayers");
    INT_SETTING(option, roundtime, "roundtime");
    INT_SETTING(option, setcpuaffinity, "setcpuaffinity");
    INT_SETTING(option, voicechat, "voicechat");
    INT_SETTING(option, voiceholdtotalk, "voiceholdtotalk");
    INT_SETTING(option, voicekey, "voicekey");
    INT_SETTING(option, voicemb, "voicemb");
    INT_SETTING(option, voicemin, "voicemin");
    INT_SETTING(option, voiceboost, "voiceboost");
    INT_SETTING(option, help, "help");
    INT_SETTING(option, occlusion, "occlusion");
#undef INT_SETTING
#undef TEXT_SETTING
    option->voiceboost_scale = (float)((float)option->voiceboost / (double)VOICE_GAIN_PERCENT);
    if (option->voiceboost_scale < VOICE_GAIN_MIN) option->voiceboost_scale = VOICE_GAIN_MIN;
    if (option->voiceboost_scale > VOICE_GAIN_MAX) option->voiceboost_scale = VOICE_GAIN_MAX;
    if (save) os_fclose(file);
    os_chdir(sr_data_sysinfo.start_dir);
}