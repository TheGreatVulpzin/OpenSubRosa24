/* Packet capture of the server (subvulpes.ini capture_packets=1). See capture.c. */
#ifndef SV_CAPTURE_H
#define SV_CAPTURE_H

/* dir 0 = received, 1 = sent. addr is the sockaddr of the peer (may be NULL). */
void sv_capture(int dir, const void *data, int len, const void *addr);

#endif /* SV_CAPTURE_H */
