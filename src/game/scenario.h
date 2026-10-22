/* Mission sets, guards and traffic. */
#ifndef SR_SCENARIO_H
#define SR_SCENARIO_H

int scenario_armhuman(int human, int type, int mags);
int scenario_createcar(int type, int color, int road, int side, int lane, float dist);
int scenario_createguards(int building, int item_type, int expire);
void scenario_createtraffic2(int n);
int scenario_createchase(int item_type, int expire);
void createmission_threeway(int unused, int n, const int *teams);
void createmission_transaction(int unused, int n, const int *teams);
int createmission_acquisition(int kind, int n, const int *teams);
void createscenario_world(void);

#endif
