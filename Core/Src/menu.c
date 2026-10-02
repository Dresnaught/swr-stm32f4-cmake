#include "menu.h"
#include "menu_main_screen.h"
#include "menu_calibration.h"
#include "menu_protection.h"
#include "menu_diagnostic.h"
#include "lcd.h"
#include "conversion.h"
#include "calibration.h"
#include "protection.h"
#include "button.h"
#include "sytick.h"
#include "adc.h"
#include "watchdog.h"
#include <stdio.h>
#include <string.h>

MenuState currentMenu = MAIN_SCREEN;
bool UIState = false; // false = REF display, true = RAD display
bool hasUnsavedConfig = false;
uint8_t buttonMenuIndex = 0;

uint16_t lastCalibratedFWD = 0;
uint16_t lastCalibratedREF = 0;
uint16_t lastCalibratedRAD = 0;
uint16_t lastCalculatedSWRValue = 100;
float lastCalculatedSWRFloatValue = 1.0f;

static const char *barStyleNames[] = {
  "OFF (Numeric)   ",
  "Scale (''''|)   ",
  "Pipes (|||||)   "
};

void lcdPrintRow(int row, const char *text) {
  lcdCursor(row, 0);
  int count = 0;
  while (*text && count < 16) {
    lcd_send_data(*text++);
    count++;
  }
  while (count < 16) {
    lcd_send_data(' ');
    count++;
  }
}

void lcdRender2RowMenu(const char *items[], uint8_t count, uint8_t selectedIndex) {
  uint8_t topIndex = (selectedIndex / 2) * 2;
  uint8_t bottomIndex = topIndex + 1;

  char row0[20];
  char row1[20];

  if (topIndex < count) {
    snprintf(row0, sizeof(row0), "%c%s", (selectedIndex == topIndex) ? '>' : ' ', items[topIndex]);
  } else {
    row0[0] = '\0';
  }
  lcdPrintRow(0, row0);

  if (bottomIndex < count) {
    snprintf(row1, sizeof(row1), "%c%s", (selectedIndex == bottomIndex) ? '>' : ' ', items[bottomIndex]);
  } else {
    row1[0] = '\0';
  }
  lcdPrintRow(1, row1);
}

uint16_t getSmartWattStep(uint16_t maxWatts, uint16_t currentWatts) {
  if (maxWatts <= 50) {
    return 1;
  } else if (maxWatts <= 100) {
    return (currentWatts < 10) ? 1 : 5;
  } else {
    // 1000W scale
    if (currentWatts < 50) return 5;
    if (currentWatts < 200) return 10;
    return 25;
  }
}

static MainMenuAction_t getMainMenuAction(uint8_t index, bool hasUnsaved) {
  if (hasUnsaved) {
    switch (index) {
      case 0: return MM_ACTION_SAVE_CONFIG;
      case 1: return MM_ACTION_CAL_FWD;
      case 2: return MM_ACTION_CAL_REF;
      case 3: return MM_ACTION_CAL_RAD;
      case 4: return MM_ACTION_PROTECTIONS;
      case 5: return MM_ACTION_DISPLAY_MODE;
      case 6: return MM_ACTION_RUNNING_TEXT;
      case 7: return MM_ACTION_DIAGNOSTIC;
      default: return MM_ACTION_BACK_MAIN;
    }
  } else {
    switch (index) {
      case 0: return MM_ACTION_CAL_FWD;
      case 1: return MM_ACTION_CAL_REF;
      case 2: return MM_ACTION_CAL_RAD;
      case 3: return MM_ACTION_PROTECTIONS;
      case 4: return MM_ACTION_DISPLAY_MODE;
      case 5: return MM_ACTION_RUNNING_TEXT;
      case 6: return MM_ACTION_DIAGNOSTIC;
      default: return MM_ACTION_BACK_MAIN;
    }
  }
}

uint8_t getMainMenuIndexForAction(MainMenuAction_t act, bool hasUnsaved) {
  if (hasUnsaved) {
    switch (act) {
      case MM_ACTION_SAVE_CONFIG:  return 0;
      case MM_ACTION_CAL_FWD:      return 1;
      case MM_ACTION_CAL_REF:      return 2;
      case MM_ACTION_CAL_RAD:      return 3;
      case MM_ACTION_PROTECTIONS:  return 4;
      case MM_ACTION_DISPLAY_MODE: return 5;
      case MM_ACTION_RUNNING_TEXT: return 6;
      case MM_ACTION_DIAGNOSTIC:   return 7;
      default:                     return 8;
    }
  } else {
    switch (act) {
      case MM_ACTION_CAL_FWD:      return 0;
      case MM_ACTION_CAL_REF:      return 1;
      case MM_ACTION_CAL_RAD:      return 2;
      case MM_ACTION_PROTECTIONS:  return 3;
      case MM_ACTION_DISPLAY_MODE: return 4;
      case MM_ACTION_RUNNING_TEXT: return 5;
      case MM_ACTION_DIAGNOSTIC:   return 6;
      default:                     return 7;
    }
  }
}

bool updateReadings(void) {
  if (currentMenu != MAIN_SCREEN) {
    return true;
  }

  // 180 ms refresh interval matches the physical rise/fall time of HD44780 liquid crystals
  static uint32_t lastRefresh = 0;
  if ((now - lastRefresh) >= 180) {
    lastRefresh = now;
    lastCalibratedFWD = calibratedFWD;
    lastCalibratedREF = calibratedREF;
    lastCalibratedRAD = calibratedRAD;
    lastCalculatedSWRValue = calculatedSWRValue;
    lastCalculatedSWRFloatValue = calculatedSWRFloatValue;
    return true;
  }

  return false;
}

void displayMenu(MenuState menu) {
  switch (menu) {
    case MAIN_SCREEN:
      renderMainScreen();
      break;

    case MAIN_MENU: {
      char title[32];
      char info[32];
      uint8_t totalMenuItems = hasUnsavedConfig ? 9 : 8;
      if (buttonMenuIndex >= totalMenuItems) buttonMenuIndex = 0;

      MainMenuAction_t act = getMainMenuAction(buttonMenuIndex, hasUnsavedConfig);

      switch (act) {
        case MM_ACTION_SAVE_CONFIG:
          snprintf(title, sizeof(title), "> Save Config");
          snprintf(info, sizeof(info), "SEL: Save to ROM");
          break;
        case MM_ACTION_CAL_FWD:
          snprintf(title, sizeof(title), "> Cal FWD");
          snprintf(info, sizeof(info), "Pts Cal: %u/%u", calGetPointCount(CAL_CH_FWD), CAL_MAX_POINTS);
          break;
        case MM_ACTION_CAL_REF:
          snprintf(title, sizeof(title), "> Cal REF");
          snprintf(info, sizeof(info), "Pts Cal: %u/%u", calGetPointCount(CAL_CH_REF), CAL_MAX_POINTS);
          break;
        case MM_ACTION_CAL_RAD:
          snprintf(title, sizeof(title), "> Cal RAD");
          snprintf(info, sizeof(info), "Pts Cal: %u/%u", calGetPointCount(CAL_CH_RAD), CAL_MAX_POINTS);
          break;
        case MM_ACTION_PROTECTIONS:
          snprintf(title, sizeof(title), "> Protections");
          if (protectionIsEnabled()) {
            snprintf(info, sizeof(info), "ON S:%u.%u R:%uW",
                     protectionGetSwrLimit() / 100, (protectionGetSwrLimit() % 100) / 10,
                     protectionGetRadLimit());
          } else {
            snprintf(info, sizeof(info), "Protection: OFF");
          }
          break;
        case MM_ACTION_DISPLAY_MODE:
          snprintf(title, sizeof(title), "> Display Mode");
          snprintf(info, sizeof(info), "%s", barStyleNames[activeCal.protection.barStyle]);
          break;
        case MM_ACTION_RUNNING_TEXT:
          snprintf(title, sizeof(title), "> Standby Text");
          snprintf(info, sizeof(info), "R Text: %s", activeCal.protection.runningTextEnabled ? "ON" : "OFF");
          break;
        case MM_ACTION_DIAGNOSTIC:
          snprintf(title, sizeof(title), "> Diagnostic");
          snprintf(info, sizeof(info), "SEL: Test System");
          break;
        case MM_ACTION_BACK_MAIN:
        default:
          snprintf(title, sizeof(title), "> Back to Main");
          snprintf(info, sizeof(info), "SEL: Main Screen");
          break;
      }

      lcdPrintRow(0, title);
      lcdPrintRow(1, info);

      if (buttonJustPressed(13)) {
        buttonMenuIndex = (buttonMenuIndex + 1) % totalMenuItems;
      } else if (buttonJustPressed(15)) {
        buttonMenuIndex = (buttonMenuIndex + totalMenuItems - 1) % totalMenuItems;
      } else if (buttonShortRelease(14)) {
        switch (act) {
          case MM_ACTION_SAVE_CONFIG:
            lcdPrintRow(0, "Saving Config...");
            lcdPrintRow(1, "Do Not Power Off");
            delay_ms(350);
            watchdogSetLocation(WDG_LOC_FLASH_WRITE);
            calSaveToFlash();
            watchdogSetLocation(WDG_LOC_MAIN_LOOP);
            hasUnsavedConfig = false;
            buttonMenuIndex = 0;
            lcdPrintRow(0, "Config Saved!   ");
            lcdPrintRow(1, "Stored in Flash ");
            delay_ms(800);
            break;
          case MM_ACTION_CAL_FWD:
            currentMenu = MAIN_CAL_FWD_MENU;
            break;
          case MM_ACTION_CAL_REF:
            currentMenu = MAIN_CAL_REF_MENU;
            break;
          case MM_ACTION_CAL_RAD:
            currentMenu = MAIN_CAL_RAD_MENU;
            break;
          case MM_ACTION_PROTECTIONS:
            currentMenu = MAIN_PROTECTION_MENU;
            break;
          case MM_ACTION_DISPLAY_MODE:
            currentMenu = MAIN_MENU_UI;
            break;
          case MM_ACTION_RUNNING_TEXT:
            activeCal.protection.runningTextEnabled = !activeCal.protection.runningTextEnabled;
            hasUnsavedConfig = true;
            buttonMenuIndex = getMainMenuIndexForAction(MM_ACTION_RUNNING_TEXT, hasUnsavedConfig);
            break;
          case MM_ACTION_DIAGNOSTIC:
            currentMenu = MAIN_DIAGNOSTIC_MENU;
            break;
          case MM_ACTION_BACK_MAIN:
            currentMenu = MAIN_SCREEN;
            break;
        }
      }
      break;
    }

    case MAIN_CAL_FWD_MENU:
      calibrationMenu(CAL_CH_FWD);
      break;

    case MAIN_CAL_REF_MENU:
      calibrationMenu(CAL_CH_REF);
      break;

    case MAIN_CAL_RAD_MENU:
      calibrationMenu(CAL_CH_RAD);
      break;

    case MAIN_PROTECTION_MENU:
      protectionMenu();
      break;

    case MAIN_DIAGNOSTIC_MENU:
      diagnosticMenu();
      break;

    case MAIN_MENU_UI: {
      uint8_t currentStyle = activeCal.protection.barStyle;
      if (currentStyle >= BAR_STYLE_COUNT) currentStyle = 0;

      char row0[20];
      char row1[20];
      snprintf(row0, sizeof(row0), "Power Bar Mode:");
      snprintf(row1, sizeof(row1), "> %s", barStyleNames[currentStyle]);
      lcdPrintRow(0, row0);
      lcdPrintRow(1, row1);

      if (buttonJustPressed(13)) {
        activeCal.protection.barStyle = (currentStyle + 1) % BAR_STYLE_COUNT;
        hasUnsavedConfig = true;
      } else if (buttonJustPressed(15)) {
        activeCal.protection.barStyle = (currentStyle + BAR_STYLE_COUNT - 1) % BAR_STYLE_COUNT;
        hasUnsavedConfig = true;
      } else if (buttonLongHold(14, 500)) {
        buttonMenuIndex = getMainMenuIndexForAction(MM_ACTION_DISPLAY_MODE, hasUnsavedConfig);
        currentMenu = MAIN_MENU;
      } else if (buttonShortRelease(14)) {
        lcdPrintRow(0, "Power Bar Mode:");
        lcdPrintRow(1, "Selected! (OK)  ");
        delay_ms(500);
      }
      break;
    }
  }
}
