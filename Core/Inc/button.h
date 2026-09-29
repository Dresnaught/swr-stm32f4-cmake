#ifndef BUTTON_H
#define BUTTON_H
#include <stdint.h>
#include <stdbool.h>

// Initializes the button GPIO pins. This function should be called once during system initialization.
void buttonInit(void);
// Reads the state of a button. Returns true if the button is pressed, false otherwise.
bool buttonRead(uint8_t button);

#endif // BUTTON_H