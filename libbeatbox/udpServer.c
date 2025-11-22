#include "udpServer.h"
#include "beatGenerator.h" // For controlling tempo/mode
#include "audioMixer.h"    // For playing sounds

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 12345
#define MAX_LINE 1024

// --- Static Variables ---
static pthread_t s_threadId;
static int s_socketFd = -1;
static volatile int s_wantQuit = 0;

// Pointers to sounds
static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;


// --- Private Functions ---

// Send a reply string back to the client
static void send_reply(const char *s, struct sockaddr_in *cli, socklen_t clen)
{
    sendto(s_socketFd, s, strlen(s), 0, (struct sockaddr*)cli, clen);
}

// Parse and execute a command
static void handle_command(const char* cmd, struct sockaddr_in *cli, socklen_t clen)
{
    char reply[MAX_LINE] = "OK\n";
    int temp = 0; // Variable to hold tempo from sscanf
    int vol = 0; // Variable to hold volume from sscanf
    if (!strcasecmp(cmd, "help") || !strcmp(cmd, "?")) {
        send_reply(
            "BeatBox Commands:\n"
            "help|?            - Show this help\n"
            "tempo +           - Increase tempo by 5\n"
            "tempo -           - Decrease tempo by 5\n"
            "tempo <num>       - Set tempo to <num> (40-300)\n"
            "mode 0|none       - Set beat mode to None\n"
            "mode 1|rock       - Set beat mode to Rock\n"
            "mode 2|custom     - Set beat mode to Custom\n"
            "play base         - Play base drum sound\n"
            "play snare        - Play snare drum sound\n"
            "play hihat        - Play hi-hat sound\n"
            "get_status        - Get current tempo and mode\n"
            "stop              - Stop the beatbox application\n",
            cli, clen);
        return;
    }
    else if (!strcasecmp(cmd, "tempo +")) {
        BeatGenerator_setTempo(BeatGenerator_getTempo() + 5);
    }
    else if (!strcasecmp(cmd, "tempo -")) {
        BeatGenerator_setTempo(BeatGenerator_getTempo() - 5);
    }
    else if (sscanf(cmd, "tempo %d", &temp) == 1) {
        BeatGenerator_setTempo(temp);
    }
    else if (!strcasecmp(cmd, "mode 0") || !strcasecmp(cmd, "mode none")) {
        BeatGenerator_setMode(BEAT_NONE);
    }
    else if (!strcasecmp(cmd, "mode 1") || !strcasecmp(cmd, "mode rock")) {
        BeatGenerator_setMode(BEAT_ROCK);
    }
    else if (!strcasecmp(cmd, "mode 2") || !strcasecmp(cmd, "mode custom")) {
        BeatGenerator_setMode(BEAT_CUSTOM);
    }
    // --- ADD THIS BLOCK for Volume ---
else if (sscanf(cmd, "volume %d", &vol) == 1) {
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;

        // This IS safe to call, because we will modify
        // the function in the next step.
        AudioMixer_setVolume(vol); // <-- UN-COMMENT THIS
        
        // We still send an "OK" reply
        snprintf(reply, sizeof(reply), "volume=%d\n", vol);
    }
    else if (!strcasecmp(cmd, "play base")) {
        AudioMixer_queueSound(s_pBaseSound);
    }
    else if (!strcasecmp(cmd, "play snare")) {
        AudioMixer_queueSound(s_pSnareSound);
    }
    else if (!strcasecmp(cmd, "play hihat")) {
        AudioMixer_queueSound(s_pHiHatSound);
    }
    else if (!strcasecmp(cmd, "get_status")) {
        int tempo = BeatGenerator_getTempo();
        BeatMode mode = BeatGenerator_getMode();
        int volume = AudioMixer_getVolume(); // Get the current volume
        
        // Update the reply to include all three values
        snprintf(reply, sizeof(reply), "tempo=%d, mode=%d, volume=%d\n", tempo, mode, volume);
    }
    else if (!strcasecmp(cmd, "stop")) {
        s_wantQuit = 1;
        snprintf(reply, sizeof(reply), "Shutting down.\n");
    }
    else {
        snprintf(reply, sizeof(reply), "Error: Unknown command '%s'\n", cmd);
    }

    send_reply(reply, cli, clen);
}

// Thread main loop
static void* udpListenerThread(void *arg)
{
    (void)arg;
    
    // Create socket
    s_socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_socketFd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(PORT);
    
    if (bind(s_socketFd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }
    
    printf("UDP server listening on port %d\n", PORT);

    // Receive loop
    char buf[MAX_LINE];
    while (!s_wantQuit) {
        struct sockaddr_in clientSin;
        socklen_t clientLen = sizeof(clientSin);
        
        ssize_t r = recvfrom(s_socketFd, buf, MAX_LINE - 1, 0,
                             (struct sockaddr*)&clientSin, &clientLen);

        if (r < 0) {
            // Error, or socket was shut down
            if (s_wantQuit) {
                break; // Normal shutdown
            }
            perror("Error receiving message");
            continue;
        }

        // Null-terminate string and trim CRLF
        while (r > 0 && (buf[r-1] == '\n' || buf[r-1] == '\r')) r--;
        buf[r] = '\0';

        if (r == 0) continue; // Ignore blank lines

        printf("UDP Server: Received: '%s'\n", buf);
        handle_command(buf, &clientSin, clientLen);
    }

    close(s_socketFd);
    s_socketFd = -1;
    return NULL;
}


// --- Public Functions ---

void UdpServer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat)
{
    // Save sound pointers
    s_pBaseSound = pBase;
    s_pSnareSound = pSnare;
    s_pHiHatSound = pHiHat;

    s_wantQuit = 0;
    pthread_create(&s_threadId, NULL, udpListenerThread, NULL);
}

void UdpServer_cleanup(void)
{
    s_wantQuit = 1;
    if (s_socketFd != -1) {
        // This is the clean shutdown from your network.c
        shutdown(s_socketFd, SHUT_RD);
    }
    pthread_join(s_threadId, NULL);
}

int UdpServer_shouldQuit(void) 
{ 
    return s_wantQuit; 
}