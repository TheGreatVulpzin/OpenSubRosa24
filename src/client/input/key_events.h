/* The system events of a frame and the letters of the text input. See key_events.c. */
#ifndef CL_INPUT_KEY_EVENTS_H
#define CL_INPUT_KEY_EVENTS_H

void __cdecl checksystemmessages(void);
int __cdecl getinputletter(int wait);

#endif /* CL_INPUT_KEY_EVENTS_H */
