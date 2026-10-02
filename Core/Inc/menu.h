#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "calibration.h"

// Reading cache for LCD display
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
  MAIN_DIAGNOSTIC_MENU,
  MAIN_MENU_UI
} MenuState;

typedef enum {
  MM_ACTION_SAVE_CONFIG,
  MM_ACTION_CAL_FWD,
  MM_ACTION_CAL_REF,
  MM_ACTION_CAL_RAD,
  MM_ACTION_PROTECTIONS,
  MM_ACTION_DISPLAY_MODE,
  MM_ACTION_RUNNING_TEXT,
  MM_ACTION_DIAGNOSTIC,
  MM_ACTION_BACK_MAIN
} MainMenuAction_t;

extern MenuState currentMenu;
extern bool UIState;
extern bool hasUnsavedConfig;
extern uint8_t buttonMenuIndex;

// Periodic sensor refresh and display tick
bool updateReadings(void);

// Main menu dispatcher
void displayMenu(MenuState menu);

// Navigation helper
uint8_t getMainMenuIndexForAction(MainMenuAction_t act, bool hasUnsaved);

// Common LCD rendering helpers shared by sub-menus
void lcdPrintRow(int row, const char *text);
void lcdRender2RowMenu(const char *items[], uint8_t count, uint8_t selectedIndex);
uint16_t getSmartWattStep(uint16_t maxWatts, uint16_t currentWatts);

#endif // MENU_H