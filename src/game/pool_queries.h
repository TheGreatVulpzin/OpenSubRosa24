/* Queries and removals for the fixed game pools and the packed bullet pool. */
#ifndef SR_POOL_QUERIES_H
#define SR_POOL_QUERIES_H

int getnextcityobject(void);
int getnexthuman(void);
int getnextitem(void);
int getnextobject(void);
int getnextplayer(void);
void deletecityobject(int index);
void deletebullet(int index);
void deleteobject(int object);

#endif
