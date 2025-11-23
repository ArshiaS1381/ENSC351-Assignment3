#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// Include our custom modules
#include "audioMixer.h"
#include "beatGenerator.h"
#include "udpServer.h"
#include "inputMan.h" 

// Correct paths for the wave files (relative to the application execution location)
// NOTE: These paths must match where your Node.js server copies the files!
#define BASE_SOUND  "beatbox-wav-files/100051__menegass__gui-drum-bd-hard.wav"
#define SNARE_SOUND "beatbox-wav-files/100059__menegass__gui-drum-snare-soft.wav"
#define HIHAT_SOUND "beatbox-wav-files/100053__menegass__gui-drum-cc.wav"

int main(void)
{
    printf("Starting BeatBox app...\n");
    
    // 1. Initialize AudioMixer
    AudioMixer_init();

    // 2. Load all sounds into memory
    wavedata_t baseSound, snareSound, hiHatSound;
    if (!AudioMixer_readWaveFileIntoMemory(BASE_SOUND, &baseSound) ||
        !AudioMixer_readWaveFileIntoMemory(SNARE_SOUND, &snareSound) ||
        !AudioMixer_readWaveFileIntoMemory(HIHAT_SOUND, &hiHatSound))
    {
        printf("ERROR: Failed to load wave files. Ensure they are in the correct path.\n");
        // Exit failure might be harsh, but necessary if no sounds can be played
        exit(EXIT_FAILURE);
    }
    printf("Sounds loaded.\n");

    // 3. Initialize Subsystems (pass sound pointers)
    
    BeatGenerator_init(&baseSound, &snareSound, &hiHatSound);
    UdpServer_init(&baseSound, &snareSound, &hiHatSound);
    InputMan_init(&baseSound, &snareSound, &hiHatSound);
    
    printf("All modules initialized.\n");

    // 4. Main Loop - blocks until UDP server receives 'stop' command
    while (!UdpServer_shouldQuit()) {
        sleep(1);
    }

    // 5. Cleanup (in reverse order of init ideally)
    printf("Shutting down...\n");

    InputMan_cleanup();
    UdpServer_cleanup();
    BeatGenerator_cleanup();
    
    // Free the memory for the sound clips
    AudioMixer_freeWaveFileData(&baseSound);
    AudioMixer_freeWaveFileData(&snareSound);
    AudioMixer_freeWaveFileData(&hiHatSound);
    
    // Close Audio
    AudioMixer_cleanup();
    
    printf("BeatBox app complete.\n");

    return 0;
}