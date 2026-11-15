/* The settings in config.txt. See options.c. */
#ifndef SR_OPTIONS_H
#define SR_OPTIONS_H

void loadsaveoptions(int save);

/* Read or write one key of config.txt. The file is the open file of a write. A read ignores it. */
void option_int(int *value, const char *key, int write, void *file);
void option_string(char *value, const char *key, int max, int write, void *file);

#endif /* SR_OPTIONS_H */
