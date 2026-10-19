/* The collision tests against the level. See collide.c. */
#ifndef SR_COLLIDE_H
#define SR_COLLIDE_H

int sphereintersectlevel(float *pos, float radius);
int lineintersectlevel(float *from, float *to, int with_objects);
int lineintersectall(float *from, float *to);
int circleintersectlevel(float *pos, float *rot, float radius);
int sphereintersectobject(int obj, float *pos, float radius);
int lineintersectobject(int obj, float *from, float *to, int with_wheels);

#endif /* SR_COLLIDE_H */
