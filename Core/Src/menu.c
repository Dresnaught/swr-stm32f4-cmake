#include "menu.h"
#include "lcd.h"
#include "conversion.h"
#include "calibration.h"
#include "button.h"
#include "sytick.h"
#include "adc.h"
#include <stdio.h>

MenuState currentMenu = MAIN_SCREEN;
bool UIState = false; // false = REF display, true = RAD (Radio-In) display

uint16_t lastCalibratedFWD = 0;
uint16_t lastCalibratedREF = 0;
uint16_t lastCalibratedRAD = 0;
float lastCalculatedSWRFloatValue = 1.0f;

static uint8_t buttonMenuIndex = 0;

// Calibration sub-state machine
typedef enum {
  CAL_STEP_MENU,            // Action selection
  CAL_STEP_PICK_POINT,      // Pick point to edit
  CAL_STEP_SET_WATT,        // Set target wattage
  CAL_STEP_PROMPT_RF,       // Ready prompt before sampling
  CAL_STEP_SAMPLING,        // Active 5-second sampling
  CAL_STEP_SAMPLE_DONE,     // Show result after sampling
  CAL_STEP_REMOVE_PICK,     // Pick point to remove
  CAL_STEP_REMOVE_CONFIRM,  // Confirm deletion
  CAL_STEP_VIEW_POINTS,     // Browse active calibration points
  CAL_STEP_STATUS_MSG       // Confirmation / alert message
} CalStepState;

static CalStepState calStep = CAL_STEP_MENU;
static bool isAddingNewPoint = false;
static uint8_t calMenuActionIndex = 0;
static uint8_t calSelectedPoint = 0;     // 0-indexed
static uint16_t calTargetWatt = 10;
static uint32_t calSamplingStartTime = 0;
static uint32_t calAdcSum = 0;
static uint32_t calAdcSampleCount = 0;
static uint8_t calLastCountSec = 255;
static uint16_t calSampledAvgAdc = 0;
static uint8_t calViewPointIndex = 0;
static const char *calStatusMsgLine0 = "";
static const char *calStatusMsgLine1 = "";

static const char *calActionItems[] = {
  "1. Add Point",
  "2. Edit Point",
  "3. Remove Point",
  "4. View Points",
  "5. Save to Flash",
  "6. Reset Def",
  "7. Back"
};
#define CAL_ACTION_COUNT 7

// Helper to write a padded line to the 1602 LCD
static void lcdPrintRow(int row, const char *text) {
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

bool updateReadings(void) {
  // Always update menu while navigating or calibrating
  if (currentMenu != MAIN_SCREEN) {
    return true;
  }

  uint16_t newCalibratedFWD = calibratedFWD;
  uint16_t newCalibratedREF = calibratedREF;
  uint16_t newCalibratedRAD = calibratedRAD;
  float newCalculatedSWRFloatValue = calculatedSWRFloatValue;

  static uint32_t lastRefresh = 0;
  bool periodicTick = (now - lastRefresh) >= 150;

  if (newCalibratedFWD != lastCalibratedFWD ||
      newCalibratedREF != lastCalibratedREF ||
      newCalibratedRAD != lastCalibratedRAD ||
      newCalculatedSWRFloatValue != lastCalculatedSWRFloatValue ||
      buttonRead(13) || buttonRead(14) || buttonRead(15) ||
      periodicTick) {

    lastRefresh = now;
    lastCalibratedFWD = newCalibratedFWD;
    lastCalibratedREF = newCalibratedREF;
    lastCalibratedRAD = newCalibratedRAD;
    lastCalculatedSWRFloatValue = newCalculatedSWRFloatValue;
    return true;
  }

  return false;
}

static uint16_t getSmartWattStep(uint16_t maxWatts, uint16_t currentWatts) {
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

void calibrationMenu(CalChannelType ch) {
  char line0[32];
  char line1[32];
  uint16_t maxW = calGetChannelMaxWatts(ch);
  const char *chName = calGetChannelName(ch);
  uint8_t count = calGetPointCount(ch);

  switch (calStep) {
    case CAL_STEP_MENU: {
      snprintf(line0, sizeof(line0), "Cal %s (%uW)", chName, maxW);
      snprintf(line1, sizeof(line1), "> %s", calActionItems[calMenuActionIndex]);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calMenuActionIndex = (calMenuActionIndex + 1) % CAL_ACTION_COUNT;
      } else if (buttonJustPressed(15)) {
        calMenuActionIndex = (calMenuActionIndex + CAL_ACTION_COUNT - 1) % CAL_ACTION_COUNT;
      } else if (buttonJustPressed(14)) {
        switch (calMenuActionIndex) {
          case 0: // 1. Add Point
            if (count >= CAL_MAX_POINTS) {
              calStatusMsgLine0 = "Table Full!";
              calStatusMsgLine1 = "Max 10 Pts Limit";
              calStep = CAL_STEP_STATUS_MSG;
            } else {
              isAddingNewPoint = true;
              calSelectedPoint = count;
              calTargetWatt = getSmartWattStep(maxW, 0);
              calStep = CAL_STEP_SET_WATT;
            }
            break;

          case 1: // 2. Edit Point
            isAddingNewPoint = false;
            calSelectedPoint = (count > 1) ? 1 : 0;
            calStep = CAL_STEP_PICK_POINT;
            break;

          case 2: // 3. Remove Point
            if (count <= CAL_MIN_POINTS) {
              calStatusMsgLine0 = "Cannot Remove!";
              calStatusMsgLine1 = "Min 2 Pts Needed";
              calStep = CAL_STEP_STATUS_MSG;
            } else {
              calSelectedPoint = (count > 1) ? count - 1 : 0;
              calStep = CAL_STEP_REMOVE_PICK;
            }
            break;

          case 3: // 4. View Points
            calViewPointIndex = 0;
            calStep = CAL_STEP_VIEW_POINTS;
            break;

          case 4: // 5. Save to Flash
            if (calSaveToFlash()) {
              calStatusMsgLine0 = "Flash Storage";
              calStatusMsgLine1 = "Saved to Flash!";
            } else {
              calStatusMsgLine0 = "Flash Storage";
              calStatusMsgLine1 = "Flash Save ERR!";
            }
            calStep = CAL_STEP_STATUS_MSG;
            break;

          case 5: // 6. Reset Def
            calResetDefaults();
            calStatusMsgLine0 = "Factory Reset";
            calStatusMsgLine1 = "Defaults Loaded!";
            calStep = CAL_STEP_STATUS_MSG;
            break;

          case 6: // 7. Back
            currentMenu = MAIN_MENU;
            calStep = CAL_STEP_MENU;
            break;
        }
      }
      break;
    }

    case CAL_STEP_PICK_POINT: {
      snprintf(line0, sizeof(line0), "%s: Edit Point", chName);
      snprintf(line1, sizeof(line1), "Point %u/%u: %4uW", 
               calSelectedPoint + 1, count, 
               activeCal.channels[ch].points[calSelectedPoint].value);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calSelectedPoint = (calSelectedPoint + 1) % count;
      } else if (buttonJustPressed(15)) {
        calSelectedPoint = (calSelectedPoint + count - 1) % count;
      } else if (buttonJustPressed(14)) {
        calTargetWatt = activeCal.channels[ch].points[calSelectedPoint].value;
        if (calTargetWatt == 0 && calSelectedPoint > 0) {
          calTargetWatt = getSmartWattStep(maxW, 0);
        }
        calStep = CAL_STEP_SET_WATT;
      }
      break;
    }

    case CAL_STEP_SET_WATT: {
      uint16_t step = getSmartWattStep(maxW, calTargetWatt);
      if (isAddingNewPoint) {
        snprintf(line0, sizeof(line0), "New Pt %u Target", count + 1);
      } else {
        snprintf(line0, sizeof(line0), "Pt %u Target Pwr", calSelectedPoint + 1);
      }
      snprintf(line1, sizeof(line1), "Watt: [ %4u ] W", calTargetWatt);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonRepeat(13)) { // UP button
        if (calTargetWatt + step <= maxW) {
          calTargetWatt += step;
        } else {
          calTargetWatt = maxW;
        }
      } else if (buttonRepeat(15)) { // DOWN button
        if (calTargetWatt >= step) {
          calTargetWatt -= step;
        } else {
          calTargetWatt = 0;
        }
      } else if (buttonJustPressed(14)) { // SELECT
        calStep = CAL_STEP_PROMPT_RF;
      }
      break;
    }

    case CAL_STEP_PROMPT_RF: {
      snprintf(line0, sizeof(line0), "Apply %uW RF", calTargetWatt);
      snprintf(line1, sizeof(line1), "SEL: Start (%us)", CAL_SAMPLE_SECONDS);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(14)) {
        calSamplingStartTime = now;
        calAdcSum = 0;
        calAdcSampleCount = 0;
        calLastCountSec = 255;
        calStep = CAL_STEP_SAMPLING;
      } else if (buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU; // Cancel
      }
      break;
    }

    case CAL_STEP_SAMPLING: {
      uint16_t rawAdc = 0;
      if (ch == CAL_CH_FWD) adcCH0Raw(&rawAdc);
      else if (ch == CAL_CH_REF) adcCH1Raw(&rawAdc);
      else if (ch == CAL_CH_RAD) adcCH2Raw(&rawAdc);

      calAdcSum += rawAdc;
      calAdcSampleCount++;

      uint32_t elapsedMs = now - calSamplingStartTime;
      uint32_t totalMs = CAL_SAMPLE_SECONDS * 1000U;

      if (elapsedMs < totalMs) {
        uint8_t secLeft = (uint8_t)((totalMs - elapsedMs + 999U) / 1000U);
        if (secLeft != calLastCountSec) {
          calLastCountSec = secLeft;
          snprintf(line0, sizeof(line0), "Sampling %s...", chName);
          snprintf(line1, sizeof(line1), "ADC:%4u  T:%1us", rawAdc, secLeft);
          lcdPrintRow(0, line0);
          lcdPrintRow(1, line1);
        }
      } else {
        // Sampling complete!
        calSampledAvgAdc = (calAdcSampleCount > 0) ? (uint16_t)(calAdcSum / calAdcSampleCount) : 0;
        if (isAddingNewPoint) {
          calAddPoint(ch, calTargetWatt, calSampledAvgAdc);
        } else {
          calUpdatePoint(ch, calSelectedPoint, calTargetWatt, calSampledAvgAdc);
        }
        calStep = CAL_STEP_SAMPLE_DONE;
      }
      break;
    }

    case CAL_STEP_SAMPLE_DONE: {
      if (isAddingNewPoint) {
        snprintf(line0, sizeof(line0), "Pt Added! (%uW)", calTargetWatt);
      } else {
        snprintf(line0, sizeof(line0), "Pt %u Done! (%uW)", calSelectedPoint + 1, calTargetWatt);
      }
      snprintf(line1, sizeof(line1), "Avg ADC: %4u", calSampledAvgAdc);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_REMOVE_PICK: {
      CalPoint_t *p = &activeCal.channels[ch].points[calSelectedPoint];
      snprintf(line0, sizeof(line0), "Remove Pt %u/%u?", calSelectedPoint + 1, count);
      snprintf(line1, sizeof(line1), "ADC:%4u W:%4u", p->raw, p->value);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calSelectedPoint = (calSelectedPoint + 1) % count;
      } else if (buttonJustPressed(15)) {
        calSelectedPoint = (calSelectedPoint + count - 1) % count;
      } else if (buttonJustPressed(14)) {
        calStep = CAL_STEP_REMOVE_CONFIRM;
      }
      break;
    }

    case CAL_STEP_REMOVE_CONFIRM: {
      snprintf(line0, sizeof(line0), "Delete Pt %u/%u?", calSelectedPoint + 1, count);
      snprintf(line1, sizeof(line1), "SEL=Yes  DOWN=No");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(14)) { // Confirm Delete
        calRemovePoint(ch, calSelectedPoint);
        calStatusMsgLine0 = "Point Removed!";
        calStatusMsgLine1 = "Table Updated";
        calStep = CAL_STEP_STATUS_MSG;
      } else if (buttonJustPressed(15)) { // Cancel
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_VIEW_POINTS: {
      CalPoint_t *p = &activeCal.channels[ch].points[calViewPointIndex];
      snprintf(line0, sizeof(line0), "%s Pt %u/%u View", chName, calViewPointIndex + 1, count);
      snprintf(line1, sizeof(line1), "Raw:%4u W:%4u", p->raw, p->value);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calViewPointIndex = (calViewPointIndex + 1) % count;
      } else if (buttonJustPressed(15)) {
        calViewPointIndex = (calViewPointIndex + count - 1) % count;
      } else if (buttonJustPressed(14)) {
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_STATUS_MSG: {
      lcdPrintRow(0, calStatusMsgLine0);
      lcdPrintRow(1, calStatusMsgLine1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU;
      }
      break;
    }
  }
}

static const char *mainMenuItemsList[] = {
  "Back to Main",
  "Cal FWD (1000W)",
  "Cal REF (100W)",
  "Cal RAD (50W)",
  "Display Mode"
};
#define MAIN_MENU_COUNT 5

void displayMenu(MenuState menu) {
  char buf0[32];
  char buf1[32];

  switch (menu) {
    case MAIN_SCREEN: {
      // Row 0: FWD Power and SWR
      snprintf(buf0, sizeof(buf0), "FWD:%4uW S:%1.2f", lastCalibratedFWD, (double)lastCalculatedSWRFloatValue);
      lcdPrintRow(0, buf0);

      // Row 1: REF or RAD (Radio-In drive power before booster)
      if (!UIState) {
        snprintf(buf1, sizeof(buf1), "REF:%4uW (Refl)", lastCalibratedREF);
      } else {
        snprintf(buf1, sizeof(buf1), "RAD:%4uW (R-In)", lastCalibratedRAD);
      }
      lcdPrintRow(1, buf1);

      if (buttonJustPressed(14)) { // SELECT opens main menu
        currentMenu = MAIN_MENU;
        buttonMenuIndex = 0;
      } else if (buttonJustPressed(13) || buttonJustPressed(15)) { // UP/DOWN toggles REF/RAD
        UIState = !UIState;
      }
      break;
    }

    case MAIN_MENU: {
      snprintf(buf0, sizeof(buf0), "> %s", mainMenuItemsList[buttonMenuIndex]);
      lcdPrintRow(0, buf0);
      lcdPrintRow(1, "SEL:Ent  UP/DN:M");

      if (buttonJustPressed(13)) {
        buttonMenuIndex = (buttonMenuIndex + 1) % MAIN_MENU_COUNT;
      } else if (buttonJustPressed(15)) {
        buttonMenuIndex = (buttonMenuIndex + MAIN_MENU_COUNT - 1) % MAIN_MENU_COUNT;
      } else if (buttonJustPressed(14)) {
        switch (buttonMenuIndex) {
          case 0:
            currentMenu = MAIN_SCREEN;
            break;
          case 1:
            currentMenu = MAIN_CAL_FWD_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            break;
          case 2:
            currentMenu = MAIN_CAL_REF_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            break;
          case 3:
            currentMenu = MAIN_CAL_RAD_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            break;
          case 4:
            currentMenu = MAIN_MENU_UI;
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

    case MAIN_MENU_UI: {
      lcdPrintRow(0, "Display Row 1:");
      if (UIState) {
        lcdPrintRow(1, "> RAD (Radio-In)");
      } else {
        lcdPrintRow(1, "> REF (Reflect)");
      }

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        UIState = !UIState;
      } else if (buttonJustPressed(14)) {
        currentMenu = MAIN_MENU;
      }
      break;
    }
  }
}
