/* The file name lists of the game. See files.c. */
#ifndef SR_FILES_H
#define SR_FILES_H

/* Fills list number "list" with the files (dirs = 0) or the folders (dirs = 1) that match the
 * pattern, sorted by name. */
void listfiles(const char *pattern, int list, int dirs);

/* Cuts each name of the list at its first dot. */
void removeflieextentions(int list);

/* The qsort order of two names: unsigned byte compare, -1, 0 or 1. */
int __cdecl comparestrings(const void *a, const void *b);

#endif /* SR_FILES_H */
