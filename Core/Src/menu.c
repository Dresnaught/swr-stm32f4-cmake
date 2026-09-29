#include "lcd.h"
#include "menu.h"
#include "conversion.h"
#include "button.h"

char* mainMenuItems[] = {
    "Back to Main",
    "Cal FWD",
    "Cal REF",
    "Cal RAD",
    "Menu UI"
};

char* calMenuItems[] = {
    "Back to Main",
    "Add Cal Point",
    "Remove Cal Point"
};
MenuState currentMenu = MAIN_SCREEN;
bool UIState = false;
uint8_t buttonMenuState = 0;
uint8_t buttonCalState = 0;
uint16_t lastCalibratedFWD = 0;
uint16_t lastCalibratedREF = 0;
uint16_t lastCalibratedRAD = 0;
float lastCalculatedSWRFloatValue = 0.0f;
bool updateReadings(void) {
    uint16_t newCalibratedFWD = calibratedFWD;
    uint16_t newCalibratedREF = calibratedREF;
    uint16_t newCalibratedRAD = calibratedRAD;
    float newCalculatedSWRFloatValue = calculatedSWRFloatValue;

    if (newCalibratedFWD != lastCalibratedFWD ||
        newCalibratedREF != lastCalibratedREF ||
        newCalibratedRAD != lastCalibratedRAD ||
        newCalculatedSWRFloatValue != lastCalculatedSWRFloatValue ||
        buttonRead(13) || buttonRead(14) || buttonRead(15)) {
        
        lastCalibratedFWD = newCalibratedFWD;
        lastCalibratedREF = newCalibratedREF;
        lastCalibratedRAD = newCalibratedRAD;
        lastCalculatedSWRFloatValue = newCalculatedSWRFloatValue;

        return true; // Readings have changed
    }
    return false; // No change in readings
}

void calibrationMenu(void) {
  lcdCursor(0, 0);
  lcdString(calMenuItems[buttonCalState]);
  if (buttonRead(13)) {
    buttonCalState = (buttonCalState + 1) % 3; // Cycle upwards through calibration menu items
  } else if (buttonRead(15)) {
    buttonCalState = (buttonCalState + 2) % 3; // Cycle backwards through calibration menu items
  } else if (buttonRead(14)) {
    switch (buttonCalState) {
      case 0:
        currentMenu = MAIN_MENU;
        break;
      case 1:
        // Add calibration point logic here
        break;
      case 2:
        // Remove calibration point logic here
        break;
      default:
        break;
    }
  }
}

void displayMenu(MenuState menu) {
    switch (menu) {
        case MAIN_SCREEN:
            lcdCursor(0, 0);
            lcdString("FWD: ");
            lcdInt(lastCalibratedFWD);
            lcdString("W    ");
            lcdCursor(0, 11);
            lcdString("S:");
            lcdFloat(lastCalculatedSWRFloatValue);
            if (!UIState) {
              lcdCursor(1, 0);
              lcdString("REF: ");
              lcdInt(lastCalibratedREF);
            } else {
              lcdCursor(1, 0);
              lcdString("RAD: ");
              lcdInt(lastCalibratedRAD);
            }
            lcdString("W    ");
            if (buttonRead(14)) {
                currentMenu = MAIN_MENU;
            }
            break;
        case MAIN_MENU:
            lcdCursor(0, 0);
            lcdString(mainMenuItems[buttonMenuState]);
            if (buttonRead(13)) {
                buttonMenuState = (buttonMenuState + 1) % 4; // Cycle upwards through menu items
            } else if (buttonRead(15)) {
              buttonMenuState = (buttonMenuState + 3) % 4; // Cycle backwards through menu items
            } else if (buttonRead(14)) {
                switch (buttonMenuState) {
                    case 0:
                        currentMenu = MAIN_SCREEN;
                        break;
                    case 1:
                        currentMenu = MAIN_CAL_FWD_MENU;
                        break;
                    case 2:
                        currentMenu = MAIN_CAL_REF_MENU;
                        break;
                    case 3:
                        currentMenu = MAIN_CAL_RAD_MENU;
                        break;
                    case 4:
                        currentMenu = MAIN_MENU_UI;
                        break;
                    default:
                        break;
                }
            }
            break;
        case MAIN_CAL_FWD_MENU:
            calibrationMenu();
            break;
        case MAIN_CAL_REF_MENU:
            calibrationMenu();
            break;
        case MAIN_CAL_RAD_MENU:
            calibrationMenu();
            break;
        case MAIN_MENU_UI:
            lcdCursor(0, 0);
            lcdString("Menu UI");
            if (buttonRead(13) || buttonRead(15)) {
                UIState = !UIState; // Toggle UI state
            } else if (buttonRead(14)) {
                currentMenu = MAIN_MENU; // Return to main menu
            }
            if (UIState) {
                lcdCursor(1, 0);
                lcdString("RAD Display");
            } else {
                lcdCursor(1, 0);
                lcdString("REF Display");
            }
            break;
        default:
            break;
    }
}


