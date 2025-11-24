#ifndef UDPSERVER_H
#define UDPSERVER_H

#include "audioMixer.h"

// Initializes the UDP listening thread.
// Takes pointers to the sounds so the "play" command can trigger them.
void UdpServer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);

// Stops the thread and closes the socket.
void UdpServer_cleanup(void);

// Returns 1 if a "stop" command has been received, 0 otherwise.
// Used by the main loop to decide when to exit.
int UdpServer_shouldQuit(void);

#endif