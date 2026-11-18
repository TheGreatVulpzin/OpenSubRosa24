/* The level editor. See level.c. */
#ifndef CL_EDITOR_LEVEL_H
#define CL_EDITOR_LEVEL_H

int __cdecl leveleditor_addintersection(int x, int y, int z);
int __cdecl leveleditor_getintersection(float x, float y, float z, float radius);
void __cdecl leveleditor_deleteintersection(int index);
int __cdecl leveleditor_addstreet(int a, int b);
void __cdecl leveleditor_deletestreet(int index);
void __cdecl level_checkeditor(void);
void __cdecl leveleditor_loop(void);

#endif /* CL_EDITOR_LEVEL_H */
