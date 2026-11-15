/* Read and write one key of the settings file. See option_keys.c. */
#ifndef CL_SETUP_OPTION_KEYS_H
#define CL_SETUP_OPTION_KEYS_H

void __cdecl optionreadwriteint(int *value, const char *key, int write);
void __cdecl optionreadwritestring(char *value, const char *key, int max, int write);

#endif /* CL_SETUP_OPTION_KEYS_H */
