#ifndef MENU_H
#define MENU_H
#include <stdint.h>
#include <stdbool.h>

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
  MAIN_MENU_UI
} MenuState;

extern MenuState currentMenu;

extern uint8_t buttonMenuState;
extern uint8_t buttonCalState;
extern char* mainMenuItems[];
extern char* calMenuItems[];
extern bool UIState;

bool updateReadings(void);
void calibrationMenu(void);
void displayMenu(MenuState menu);

#endif // MENU_H