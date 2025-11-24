#ifndef JOYSTICK_H
#define JOYSTICK_H

// Initialize joystick resources (if any).
void Joystick_init(void);

// Clean up resources.
void Joystick_cleanup(void);

// Reads the joystick position and interprets it as a volume command.
// Returns:
//   1 : Joystick is UP (Increase Volume)
//  -1 : Joystick is DOWN (Decrease Volume)
//   0 : Joystick is CENTERED (No change)
int Joystick_readVolumeDirection(void);

#endif