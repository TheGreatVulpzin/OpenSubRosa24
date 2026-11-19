/* The folders of the client: the folder where it starts and the folder of its settings. */
#include "setup/paths.h"

#include "client.h"
#include "sr_runtime.h"

#define PATH_SIZE 0x200
#define CSIDL_PERSONAL 5
#define SETTINGS_FOLDER "Sub Rosa"

void *sr_import(const char *dll, const char *name);

/* Keep the start folder, then make the settings folder inside the documents folder of the user
 * when it is not there. The settings path is the documents path, a slash and the folder name. The
 * program ends in its start folder. */
void __cdecl getpaths(void)
{
    char *(__cdecl * get_cwd)(char *, int) = (char *(__cdecl *)(char *, int))sr_import("MSVCR100.DLL", "_getcwd");
    int(__cdecl * change_dir)(const char *) = (int(__cdecl *)(const char *))sr_import("MSVCR100.DLL", "_chdir");
    int(__cdecl * make_dir)(const char *) = (int(__cdecl *)(const char *))sr_import("MSVCR100.DLL", "_mkdir");
    char *(__cdecl * join)(char *, const char *) = (char *(__cdecl *)(char *, const char *))sr_import("MSVCR100.DLL", "strcat");
    int(__stdcall * special_folder)(void *, char *, int, int) =
        (int(__stdcall *)(void *, char *, int, int))sr_import("SHELL32.DLL", "SHGetSpecialFolderPathA");

    get_cwd(sr_data_sysinfo.start_dir, PATH_SIZE);
    special_folder(NULL, sr_data_sysinfo.config_dir, CSIDL_PERSONAL, 0);
    change_dir(sr_data_sysinfo.config_dir);
    if (change_dir(SETTINGS_FOLDER) == -1) {
        make_dir(SETTINGS_FOLDER);
        change_dir(SETTINGS_FOLDER);
    }
    join(sr_data_sysinfo.config_dir, "/");
    join(sr_data_sysinfo.config_dir, SETTINGS_FOLDER);
    change_dir(sr_data_sysinfo.start_dir);
}
