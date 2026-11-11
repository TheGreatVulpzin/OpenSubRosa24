/* The round indicators of the 2D layer. See indicators.c. */
#ifndef CL_RENDER_INDICATORS_H
#define CL_RENDER_INDICATORS_H

void __cdecl drawplayerindicator(float x, float y, float radius, float red, float green, float blue, float alpha,
                                 float angle);
void __cdecl drawcircleindicator(float x, float y, float radius, float needle);

#endif /* CL_RENDER_INDICATORS_H */
