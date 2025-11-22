#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "audioMixer.h"
#include "beatGenerator.h"
#include "udpServer.h"  // <-- This provides the function declarations

// Correct paths from your project structure
#define BASE_SOUND   "../assets/wave-files/100051__menegass__gui-drum-bd-hard.wav"
#define SNARE_SOUND  "../assets/wave-files/100059__menegass__gui-drum-snare-soft.wav"
#define HIHAT_SOUND  "../assets/wave-files/100053__menegass__gui-drum-cc.wav"

int main(void)
{
    printf("Starting BeatBox app...\n");
    
    // 1. Initialize AudioMixer
    //    (Make sure you commented out setVolume() in audioMixer.c!)
    AudioMixer_init();

    // 2. Load all sounds
    wavedata_t baseSound, snareSound, hiHatSound;
    if (!AudioMixer_readWaveFileIntoMemory(BASE_SOUND, &baseSound) ||
        !AudioMixer_readWaveFileIntoMemory(SNARE_SOUND, &snareSound) ||
        !AudioMixer_readWaveFileIntoMemory(HIHAT_SOUND, &hiHatSound))
    {
        printf("ERROR: Failed to load wave files. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    printf("Sounds loaded.\n");

    // 3. Initialize modules
    BeatGenerator_init(&baseSound, &snareSound, &hiHatSound);
    
    // --- FIX 1 ---
    // Pass the sound pointers to the UDP server
    UdpServer_init(&baseSound, &snareSound, &hiHatSound);
    
    printf("Beat generator and UDP server started.\n");

    // 4. Wait for shutdown command
    printf("Running... Waiting for 'stop' command via UDP.\n");
    
    // --- FIX 2 ---
    // The function is named UdpServer_shouldQuit()
    while (!UdpServer_shouldQuit()) {
        sleep(1);
    }

    // 5. --- Cleanup ---
    printf("Shutting down...\n");
    UdpServer_cleanup();
    BeatGenerator_cleanup();
    
    AudioMixer_freeWaveFileData(&baseSound);
    AudioMixer_freeWaveFileData(&snareSound);
    AudioMixer_freeWaveFileData(&hiHatSound);
    
    AudioMixer_cleanup();
    printf("BeatBox app complete.\n");

    return 0;
}