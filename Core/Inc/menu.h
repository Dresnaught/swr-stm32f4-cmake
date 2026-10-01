#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "calibration.h"

extern uint16_t lastCalibratedFWD;
extern uint16_t lastCalibratedREF;
extern uint16_t lastCalibratedRAD;
extern uint16_t lastCalculatedSWRValue;
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

// Screen saver / running text configuration
#define SCREEN_SAVER_TIMEOUT_MS   30000U // 30 seconds of inactivity
#define SCREEN_SAVER_SCROLL_MS    250U   // Marquee step speed (250ms per shift)
#define SCREEN_SAVER_ROW0_TEXT    "SWR & Power Mtr " // Static text on Cursor (0,0)

// Protection menu handler
void protectionMenu(void);

// Screen saver running text updater
void updateRunningText(void);

#endif // MENU_H