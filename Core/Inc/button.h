#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

// Initializes button GPIO pins on PB13-PB15 with internal pull-ups
void buttonInit(void);

// Updates debouncer states across PB13-PB15
void buttonUpdate(void);

// Clears all pending edge triggers, repeat timers, and armed releases across all buttons
void buttonClearAll(void);

// Raw level read: returns true while the button is pressed (active low)
bool buttonRead(uint8_t button);

// Fast check: returns true if any of the 3 buttons are pressed down (raw register check)
bool buttonAnyPressed(void);

// Single edge trigger: returns true once on physical press-down (clean 25ms debounced edge)
bool buttonJustPressed(uint8_t button);

// Repeat trigger: returns true on first press, and repeats periodically if held
bool buttonRepeat(uint8_t button);

// Hold trigger: returns true once when held continuously for at least holdMs
bool buttonLongHold(uint8_t button, uint32_t holdMs);

// Short release trigger: returns true on release only if released before holdMs
bool buttonShortRelease(uint8_t button);

#endif // BUTTON_H