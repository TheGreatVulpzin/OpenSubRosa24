/* The folders of the client: the folder where it starts and the folder of its settings. */
#include <direct.h>
#include <string.h>
#include "system_libs.h"
#include "setup/paths.h"

#include "client.h"
#include "sr_runtime.h"

#define PATH_SIZE 0x200
#define CSIDL_PERSONAL 5
#define SETTINGS_FOLDER "Sub Rosa"

/* Keep the start folder, then make the settings folder inside the documents folder of the user
 * when it is not there. The settings path is the documents path, a slash and the folder name. The
 * program ends in its start folder. */
void __cdecl getpaths(void)
{
    _getcwd(sr_data_sysinfo.start_dir, PATH_SIZE);
    SHGetSpecialFolderPathA(NULL, sr_data_sysinfo.config_dir, CSIDL_PERSONAL, 0);
    _chdir(sr_data_sysinfo.config_dir);
    if (_chdir(SETTINGS_FOLDER) == -1) {
        _mkdir(SETTINGS_FOLDER);
        _chdir(SETTINGS_FOLDER);
    }
    strcat(sr_data_sysinfo.config_dir, "/");
    strcat(sr_data_sysinfo.config_dir, SETTINGS_FOLDER);
    _chdir(sr_data_sysinfo.start_dir);
}
