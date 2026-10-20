/* The text parser of the game. See parser.c. */
#ifndef SR_PARSER_H
#define SR_PARSER_H

void loadtextfile(const char *path);
int checkstring(const char *key);
int findstring(const char *key);
int getint(void);
void getstring(char *out, int max);
void cs_getline(char *out, int max);

#endif /* SR_PARSER_H */
