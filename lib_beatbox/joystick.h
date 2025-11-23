#ifndef JOYSTICK_H
#define JOYSTICK_H

void Joystick_init(void);
void Joystick_cleanup(void);

// Returns 1 for UP (+5 volume), -1 for DOWN (-5 volume), 0 otherwise
int Joystick_readVolumeDirection(void);

#endif