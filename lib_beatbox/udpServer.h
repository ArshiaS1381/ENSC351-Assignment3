#ifndef UDPSERVER_H
#define UDPSERVER_H

#include "audioMixer.h"

// The UDP port must match the one used by beatbox_server.js (12345)

void UdpServer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);
void UdpServer_cleanup(void);

// Returns true (1) if the stop command has been received
int UdpServer_shouldQuit(void);

#endif