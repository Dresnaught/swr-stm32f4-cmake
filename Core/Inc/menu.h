#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "calibration.h"

extern uint16_t lastCalibratedFWD;
extern uint16_t lastCalibratedREF;
extern uint16_t lastCalibratedRAD;
extern float lastCalculatedSWRFloatValue;

typedef enum {
  MAIN_SCREEN,
  MAIN_MENU,
  MAIN_CAL_FWD_MENU,
  MAIN_CAL_REF_MENU,
  MAIN_CAL_RAD_MENU,
  MAIN_PROTECTION_MENU,
  MAIN_MENU_UI
} MenuState;

extern MenuState currentMenu;
extern bool UIState; // false = REF power displayed, true = RAD (Radio-In) power displayed

// Checks sensor values, timer ticks, and button state
bool updateReadings(void);

// Main menu dispatcher
void displayMenu(MenuState menu);

// Calibration menu handler
void calibrationMenu(CalChannelType ch);

// Protection menu handler
void protectionMenu(void);

#endif // MENU_H