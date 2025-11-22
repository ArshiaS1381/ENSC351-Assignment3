#include <stdio.h>
#include <unistd.h>
#include "audioMixer.h"

int main(void)
{
    printf("Initializing audio mixer...\n");
    AudioMixer_init();
    while(1)
    {   
        sleep(1);
    }
    printf("Cleaning up...\n");
    AudioMixer_cleanup();
    return 0;
}