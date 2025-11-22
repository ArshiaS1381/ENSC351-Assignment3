#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "audioMixer.h" // For wavedata_t

// Starts the UDP listener thread.
// Takes pointers to the sounds it can play.
void UdpServer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);

// Stops the listener thread and closes the socket.
void UdpServer_cleanup(void);

// Returns 1 (true) if a client has sent the 'stop' command, 0 otherwise.
int  UdpServer_shouldQuit(void);

#endif