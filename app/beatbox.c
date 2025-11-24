/*
 * BeatBox Main Application
 * * This is the entry point for the BeatBox application.
 * It is responsible for initializing the subsystems (Audio, Beat Gen, Input, UDP),
 * loading the necessary resources (WAV files), and maintaining the main thread
 * alive until a shutdown signal is received.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// Module includes
#include "audioMixer.h"
#include "beatGenerator.h"
#include "udpServer.h"
#include "inputMan.h" 

// --- Configuration Constants ---

// The path to the WAV files relative to the executable.
// Ensure these files exist in the target directory or the app will fail to start.
// Matches the assignment folder structure: ~/ensc351/public/myApps/beatbox-wav-files/
#define FILE_PATH_BASE   "beatbox-wav-files/100051__menegass__gui-drum-bd-hard.wav"
#define FILE_PATH_SNARE  "beatbox-wav-files/100059__menegass__gui-drum-snare-soft.wav"
#define FILE_PATH_HIHAT  "beatbox-wav-files/100053__menegass__gui-drum-cc.wav"

int main(void)
{
    printf("Starting BeatBox app...\n");
    
    // 1. Initialize the Audio Subsystem first
    // We need the mixer ready before we can load any sound data into it.
    AudioMixer_init();

    // 2. Load the drum sounds into memory
    // These calls read the WAV files from the disk and store the PCM data
    // in structs that the mixer can access quickly during playback.
    wavedata_t baseSound, snareSound, hiHatSound;
    
    if (!AudioMixer_readWaveFileIntoMemory(FILE_PATH_BASE, &baseSound) ||
        !AudioMixer_readWaveFileIntoMemory(FILE_PATH_SNARE, &snareSound) ||
        !AudioMixer_readWaveFileIntoMemory(FILE_PATH_HIHAT, &hiHatSound))
    {
        printf("ERROR: Failed to load wave files.\n");
        printf("  Ensure the 'beatbox-wav-files' folder is in the same directory as the executable.\n");
        // We cannot proceed without audio assets.
        exit(EXIT_FAILURE);
    }
    printf("Audio assets loaded successfully.\n");

    // 3. Initialize Control Modules
    // We pass pointers to the loaded sounds so these modules can trigger
    // playback without needing to know about file paths or memory management.
    BeatGenerator_init(&baseSound, &snareSound, &hiHatSound);
    
    // Initialize UDP Server (Listens on Port 12345 for Node.js commands)
    UdpServer_init(&baseSound, &snareSound, &hiHatSound);
    
    // Initialize Input Manager (Handles Joystick, Rotary Encoder, Accelerometer)
    InputMan_init(&baseSound, &snareSound, &hiHatSound);
    
    printf("BeatBox fully initialized. Entering main loop.\n");

    // 4. Main Event Loop
    // The main thread's only job now is to wait. The actual work is being done
    // by the pthreads created in the Init functions above.
    // We check the UDP server status to see if a remote shutdown command was sent.
    while (!UdpServer_shouldQuit()) {
        sleep(1); 
    }

    // 5. Cleanup Sequence
    // Shut down in reverse order of initialization to ensure dependencies 
    // (like the audio mixer) stay alive as long as other modules might need them.
    printf("Shutdown signal received. Cleaning up...\n");

    InputMan_cleanup();
    UdpServer_cleanup();
    BeatGenerator_cleanup();
    
    // Release the memory holding the raw PCM audio data
    AudioMixer_freeWaveFileData(&baseSound);
    AudioMixer_freeWaveFileData(&snareSound);
    AudioMixer_freeWaveFileData(&hiHatSound);
    
    // Finally, close the ALSA PCM handle and kill the playback thread
    AudioMixer_cleanup();
    
    printf("BeatBox app shutdown complete.\n");

    return 0;
}