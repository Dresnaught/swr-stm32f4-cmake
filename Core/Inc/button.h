#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// Initializes the button GPIO pins on PB13-PB15
void buttonInit(void);

// Raw level read: returns true while the button is pressed (active low)
bool buttonRead(uint8_t button);

// Single edge trigger: returns true only once per physical press (debounced)
bool buttonJustPressed(uint8_t button);

// Repeat trigger: returns true on first press, and repeats periodically if held
bool buttonRepeat(uint8_t button);

// Hold trigger: returns true once when held continuously for at least holdMs
bool buttonLongHold(uint8_t button, uint32_t holdMs);

// Short release trigger: returns true on release if pressed and released before holdMs
bool buttonShortRelease(uint8_t button);

#endif // BUTTON_H