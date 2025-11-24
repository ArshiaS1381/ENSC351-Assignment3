/*
 * UDP Server Module
 * * This module implements a simple UDP server to allow external control of the BeatBox.
 * It runs in its own thread and listens for text-based commands (volume, tempo, mode, play).
 * It parses these commands and calls the appropriate functions in other modules.
 * * It supports both "Getter" and "Setter" styles:
 * - "volume"      -> Returns current volume
 * - "volume 50"   -> Sets volume to 50 and returns new value
 */

#include "udpServer.h"
#include "beatGenerator.h" 
#include "audioMixer.h"  
#include "inputMan.h"  
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>

// --- Configuration Constants ---

#define UDP_PORT 12345        // Port to listen on (must match Node.js server)
#define RX_BUFFER_SIZE 1024   // Max size of a single UDP packet

// --- Internal State ---

static pthread_t s_threadId;
static int s_socketFd = -1;
static volatile bool s_wantQuit = false;

// Pointers to the sound data for the "play" command
static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;

// --- Private Helpers ---

// Helper to send a string response back to the sender
static void send_reply(const char *s, struct sockaddr_in *cli, socklen_t clen) {
    if (s_socketFd != -1) {
        sendto(s_socketFd, s, strlen(s), 0, (struct sockaddr*)cli, clen);
    }
}

// Command Parser
// Decodes the text command and executes the corresponding action.
static void handle_command(char* cmd, struct sockaddr_in *cli, socklen_t clen) {
    char reply[RX_BUFFER_SIZE] = "";
    
    // --- VOLUME Command ---
    if (strncmp(cmd, "volume", 6) == 0) {
        int newVol;
        // Try to read an integer argument. If successful (ret == 1), it's a SET command.
        if (sscanf(cmd, "volume %d", &newVol) == 1) {
            AudioMixer_setVolume(newVol);
            // Notify InputMan to lock out the joystick temporarily so it doesn't fight us.
            InputMan_notifyManualVolumeSet(); 
            sprintf(reply, "%d", AudioMixer_getVolume());
        } 
        // If no argument found, treat as a GET command.
        else {
            sprintf(reply, "%d", AudioMixer_getVolume());
        }
    }
    // --- TEMPO Command ---
    else if (strncmp(cmd, "tempo", 5) == 0) {
        int newTempo;
        if (sscanf(cmd, "tempo %d", &newTempo) == 1) {
            BeatGenerator_setTempo(newTempo);
            sprintf(reply, "%d", BeatGenerator_getTempo());
        }
        else {
            sprintf(reply, "%d", BeatGenerator_getTempo());
        }
    }
    // --- MODE Command ---
    else if (strncmp(cmd, "mode", 4) == 0) {
        int newMode;
        if (sscanf(cmd, "mode %d", &newMode) == 1) {
            BeatGenerator_setMode((BeatMode)newMode);
            sprintf(reply, "%d", newMode);
        }
        else {
            sprintf(reply, "%d", BeatGenerator_getMode());
        }
    }
    // --- PLAY Command ---
    // Allows the web interface to trigger individual drum sounds
    else if (strncmp(cmd, "play", 4) == 0) {
        int soundId;
        if (sscanf(cmd, "play %d", &soundId) == 1) {
             switch(soundId) {
                case 0: AudioMixer_queueSound(s_pBaseSound); break;
                case 1: AudioMixer_queueSound(s_pHiHatSound); break;
                case 2: AudioMixer_queueSound(s_pSnareSound); break;
            }
        }
        sprintf(reply, "1"); // Acknowledge
    }
    // --- STOP Command ---
    // Terminates the main application loop
    else if (strncmp(cmd, "stop", 4) == 0) {
        s_wantQuit = true;
        sprintf(reply, "Stopping");
    }
    else {
        sprintf(reply, "Error: Unknown command");
    }

    send_reply(reply, cli, clen);
}

// --- Main UDP Thread ---

static void* udpListenerThread(void *arg) {
    (void)arg;
    
    // 1. Create Socket
    if ((s_socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP: socket failed");
        return NULL;
    }

    // 2. Bind to Port
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family    = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(s_socketFd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("UDP: bind failed");
        close(s_socketFd);
        s_socketFd = -1;
        return NULL;
    }
    
    printf("UDP Server listening on port %d...\n", UDP_PORT);

    char buf[RX_BUFFER_SIZE];
    struct sockaddr_in clientSin;
    socklen_t clientLen = sizeof(clientSin);
    
    // 3. Listen Loop
    while (!s_wantQuit) {
        // Blocks here until data arrives
        ssize_t r = recvfrom(s_socketFd, buf, RX_BUFFER_SIZE - 1, 0,
                             (struct sockaddr*)&clientSin, &clientLen);

        if (r < 0) {
            // Check if we were woken up by a shutdown signal
            if (s_wantQuit) break; 
            perror("UDP: Error receiving");
            continue;
        }

        // Null-terminate the received string
        buf[r] = '\0';
        
        // Clean up whitespace (newlines) from the end of the command
        while (r > 0 && (buf[r-1] == '\n' || buf[r-1] == '\r')) buf[--r] = '\0';

        if (r > 0) {
            handle_command(buf, &clientSin, clientLen);
        }
    }

    close(s_socketFd);
    s_socketFd = -1;
    return NULL;
}

// --- Public API ---

void UdpServer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat) {
    s_pBaseSound = pBase;
    s_pSnareSound = pSnare;
    s_pHiHatSound = pHiHat;
    s_wantQuit = false;
    pthread_create(&s_threadId, NULL, udpListenerThread, NULL);
}

void UdpServer_cleanup(void) {
    s_wantQuit = true;
    if (s_socketFd != -1) {
        // Shutdown the socket to force recvfrom() to unblock and return
        shutdown(s_socketFd, SHUT_RD);
    }
    pthread_join(s_threadId, NULL);
}

int UdpServer_shouldQuit(void) {
    return s_wantQuit ? 1 : 0;
}