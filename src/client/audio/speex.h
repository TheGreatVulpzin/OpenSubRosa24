/* Voice encoder and decoder state. */
#ifndef CL_AUDIO_SPEEX_H
#define CL_AUDIO_SPEEX_H
void enablespeex(void);
void disablespeex(void);
void preprocessspeex(int channel, int player, int frame);
void encodespeex(int channel, int player, int frame);
#endif
