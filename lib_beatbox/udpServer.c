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

#define PORT 12345
#define MAX_LINE 1024

static pthread_t s_threadId;
static int s_socketFd = -1;
static volatile bool s_wantQuit = false;

static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;

static void send_reply(const char *s, struct sockaddr_in *cli, socklen_t clen) {
    if (s_socketFd != -1) {
        sendto(s_socketFd, s, strlen(s), 0, (struct sockaddr*)cli, clen);
    }
}

static void handle_command(char* cmd, struct sockaddr_in *cli, socklen_t clen) {
    char reply[MAX_LINE] = "";
    
    // --- VOLUME (GET: "volume undefined", SET: "volume 80") ---
    if (strncmp(cmd, "volume", 6) == 0) {
        int newVol;
        if (strstr(cmd, "undefined") != NULL) {
            // GET request
            sprintf(reply, "%d", AudioMixer_getVolume());
        } 
        else if (sscanf(cmd, "volume %d", &newVol) == 1) {
            // SET request
            AudioMixer_setVolume(newVol);
            InputMan_notifyManualVolumeSet(); // CRITICAL: Stop Accel/Joystick override
            sprintf(reply, "%d", AudioMixer_getVolume());
        }
    }
    // --- TEMPO (GET: "tempo undefined", SET: "tempo 100") ---
    else if (strncmp(cmd, "tempo", 5) == 0) {
        int newTempo;
        if (strstr(cmd, "undefined") != NULL) {
            // GET request
            sprintf(reply, "%d", BeatGenerator_getTempo());
        }
        else if (sscanf(cmd, "tempo %d", &newTempo) == 1) {
            // SET request
            BeatGenerator_setTempo(newTempo);
            sprintf(reply, "%d", BeatGenerator_getTempo());
        }
    }
    // --- MODE (GET: "mode undefined", SET: "mode 2") ---
    else if (strncmp(cmd, "mode", 4) == 0) {
        int newMode;
        if (strstr(cmd, "undefined") != NULL) {
            // GET request
            sprintf(reply, "%d", BeatGenerator_getMode());
        }
        else if (sscanf(cmd, "mode %d", &newMode) == 1) {
            // SET request
            BeatGenerator_setMode((BeatMode)newMode);
            sprintf(reply, "%d", newMode);
        }
    }
    // --- PLAY (SET: "play 1") ---
    else if (strncmp(cmd, "play", 4) == 0) {
        int soundId;
        if (sscanf(cmd, "play %d", &soundId) == 1) {
            // play 0=base, 1=hi-hat, 2=snare (based on beatbox_ui.js)
            switch(soundId) {
                case 0: AudioMixer_queueSound(s_pBaseSound); break;
                case 1: AudioMixer_queueSound(s_pHiHatSound); break;
                case 2: AudioMixer_queueSound(s_pSnareSound); break;
            }
        }
        sprintf(reply, "1"); 
    }
    // --- STOP (SET: "stop") ---
    else if (strncmp(cmd, "stop", 4) == 0) {
        s_wantQuit = true;
        sprintf(reply, "Stopping");
    }
    else {
        sprintf(reply, "Error: Unknown command");
    }

    send_reply(reply, cli, clen);
}

static void* udpListenerThread(void *arg) {
    (void)arg;
    
    // Set up socket and binding
    if ((s_socketFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP: socket failed");
        return NULL;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family    = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(s_socketFd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("UDP: bind failed");
        close(s_socketFd);
        s_socketFd = -1;
        return NULL;
    }
    
    printf("UDP Server listening on port %d...\n", PORT);

    char buf[MAX_LINE];
    struct sockaddr_in clientSin;
    socklen_t clientLen = sizeof(clientSin);
    
    while (!s_wantQuit) {
        ssize_t r = recvfrom(s_socketFd, buf, MAX_LINE - 1, 0,
                             (struct sockaddr*)&clientSin, &clientLen);

        if (r < 0) {
            if (s_wantQuit) break; 
            perror("UDP: Error receiving");
            continue;
        }

        buf[r] = '\0';
        // Trim newline/carriage returns
        while (r > 0 && (buf[r-1] == '\n' || buf[r-1] == '\r')) buf[--r] = '\0';

        if (r > 0) {
            handle_command(buf, &clientSin, clientLen);
        }
    }

    close(s_socketFd);
    s_socketFd = -1;
    return NULL;
}

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
        // Shutdown read end to unblock recvfrom()
        shutdown(s_socketFd, SHUT_RD);
    }
    pthread_join(s_threadId, NULL);
}

int UdpServer_shouldQuit(void) {
    return s_wantQuit ? 1 : 0;
}