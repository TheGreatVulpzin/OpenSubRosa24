/* Voice input and speaker selection. */
#ifndef CL_AUDIO_VOICE_H
#define CL_AUDIO_VOICE_H
void getaudioinput(int player);
void addvoice(int listener, int player, int human, int item, float distance);
int getnextvoice(int listener, float distance);
#endif
