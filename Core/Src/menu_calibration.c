#include "menu_calibration.h"
#include "menu.h"
#include "lcd.h"
#include "button.h"
#include "sytick.h"
#include "adc.h"
#include "watchdog.h"
#include <stdio.h>
#include <string.h>

typedef enum {
  CAL_STEP_MENU,
  CAL_STEP_PICK_POINT,
  CAL_STEP_SET_WATT,
  CAL_STEP_PROMPT_RF,
  CAL_STEP_SAMPLING,
  CAL_STEP_ADAPT_PROMPT,
  CAL_STEP_CONFLICT_WARN,
  CAL_STEP_SAMPLE_DONE,
  CAL_STEP_REMOVE_PICK,
  CAL_STEP_REMOVE_CONFIRM,
  CAL_STEP_REMOVE_ALL_CONFIRM,
  CAL_STEP_VIEW_POINTS,
  CAL_STEP_STATUS_MSG,
  CAL_STEP_SAVE_PROMPT
} CalStepState;

static CalStepState calStep = CAL_STEP_MENU;
static bool isAddingNewPoint = false;
static uint8_t calMenuActionIndex = 0;
static uint8_t calSelectedPoint = 0;
static uint16_t calTargetWatt = 10;
static uint32_t calSamplingStartTime = 0;
static uint32_t calAdcSum = 0;
static uint32_t calAdcSampleCount = 0;
static uint8_t calLastCountSec = 255;
static uint16_t calSampledAvgAdc = 0;
static uint8_t calViewPointIndex = 0;
static const char *calStatusMsgLine0 = "";
static const char *calStatusMsgLine1 = "";
static bool calHasUnsavedChanges = false;
static uint8_t savePromptChoice = 0;

static uint8_t calAdaptChoice = 0;
static uint8_t calConflictPointIdx = 0;
static uint8_t calConflictChoice = 0;

static const char *calActionItems[] = {
  "1.Add Point",
  "2.Edit Point",
  "3.Remove Point",
  "4.View Points",
  "5.Save Flash",
  "6.Reset Def",
  "7.Back"
};
#define CAL_ACTION_COUNT 7

void calibrationMenu(CalChannelType ch) {
  char line0[32];
  char line1[32];
  uint16_t maxW = calGetChannelMaxWatts(ch);
  const char *chName = calGetChannelName(ch);
  uint8_t count = calGetPointCount(ch);

  switch (calStep) {
    case CAL_STEP_MENU: {
      lcdRender2RowMenu(calActionItems, CAL_ACTION_COUNT, calMenuActionIndex);

      if (buttonJustPressed(13)) {
        calMenuActionIndex = (calMenuActionIndex + 1) % CAL_ACTION_COUNT;
      } else if (buttonJustPressed(15)) {
        calMenuActionIndex = (calMenuActionIndex + CAL_ACTION_COUNT - 1) % CAL_ACTION_COUNT;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
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

          case 4: // 5. Save Flash
            lcdPrintRow(0, "Saving Config...");
            lcdPrintRow(1, "Do Not Power Off");
            delay_ms(350);
            watchdogSetLocation(WDG_LOC_FLASH_WRITE);
            if (calSaveToFlash()) {
              calHasUnsavedChanges = false;
              hasUnsavedConfig = false;
              calStatusMsgLine0 = "Flash Storage";
              calStatusMsgLine1 = "Saved to Flash!";
            } else {
              calStatusMsgLine0 = "Flash Storage";
              calStatusMsgLine1 = "Flash Save ERR!";
            }
            watchdogSetLocation(WDG_LOC_MAIN_LOOP);
            calStep = CAL_STEP_STATUS_MSG;
            break;

          case 5: // 6. Reset Defaults
            calResetDefaults();
            calHasUnsavedChanges = true;
            hasUnsavedConfig = true;
            calStatusMsgLine0 = "Curves Reset!";
            calStatusMsgLine1 = "Reloaded Default";
            calStep = CAL_STEP_STATUS_MSG;
            break;

          case 6: // 7. Back
            if (calHasUnsavedChanges) {
              savePromptChoice = 0;
              calStep = CAL_STEP_SAVE_PROMPT;
            } else {
              MainMenuAction_t act = (ch == CAL_CH_FWD) ? MM_ACTION_CAL_FWD :
                                     (ch == CAL_CH_REF) ? MM_ACTION_CAL_REF : MM_ACTION_CAL_RAD;
              buttonMenuIndex = getMainMenuIndexForAction(act, hasUnsavedConfig);
              currentMenu = MAIN_MENU;
            }
            break;
        }
      }
      break;
    }

    case CAL_STEP_PICK_POINT: {
      if (calSelectedPoint >= count) calSelectedPoint = 0;
      CalPoint_t *p = &activeCal.channels[ch].points[calSelectedPoint];
      snprintf(line0, sizeof(line0), "Edit %s Pt %u/%u", chName, calSelectedPoint + 1, count);
      snprintf(line1, sizeof(line1), "ADC:%4u W:%4u", p->raw, p->value);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calSelectedPoint = (calSelectedPoint + 1) % count;
      } else if (buttonJustPressed(15)) {
        calSelectedPoint = (calSelectedPoint + count - 1) % count;
      } else if (buttonLongHold(14, 500)) {
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        calTargetWatt = p->value;
        calStep = CAL_STEP_SET_WATT;
      }
      break;
    }

    case CAL_STEP_SET_WATT: {
      snprintf(line0, sizeof(line0), "%s Target Power", chName);
      snprintf(line1, sizeof(line1), "Set: [ %4u W ]", calTargetWatt);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      uint16_t step = getSmartWattStep(maxW, calTargetWatt);

      if (buttonRepeat(13)) { // Increment
        if (calTargetWatt + step <= maxW) calTargetWatt += step;
        else calTargetWatt = maxW;
      } else if (buttonRepeat(15)) { // Decrement
        if (calTargetWatt >= step + 1) calTargetWatt -= step;
        else calTargetWatt = 1;
      } else if (buttonLongHold(14, 500)) {
        calStep = isAddingNewPoint ? CAL_STEP_MENU : CAL_STEP_PICK_POINT;
      } else if (buttonShortRelease(14)) {
        calStep = CAL_STEP_PROMPT_RF;
      }
      break;
    }

    case CAL_STEP_PROMPT_RF: {
      snprintf(line0, sizeof(line0), "Apply %uW RF", calTargetWatt);
      snprintf(line1, sizeof(line1), "SEL:Run  DN:Back");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonLongHold(14, 500) || buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        calSamplingStartTime = now;
        calAdcSum = 0;
        calAdcSampleCount = 0;
        calLastCountSec = 255;
        calStep = CAL_STEP_SAMPLING;
      }
      break;
    }

    case CAL_STEP_SAMPLING: {
      watchdogSetLocation(WDG_LOC_CAL_SAMPLE);
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
          snprintf(line1, sizeof(line1), "ADC:%4u Time:%1us", rawAdc, secLeft);
          lcdPrintRow(0, line0);
          lcdPrintRow(1, line1);
        }
      } else {
        calSampledAvgAdc = (calAdcSampleCount > 0) ? (uint16_t)(calAdcSum / calAdcSampleCount) : 0;
        calAdaptChoice = 0; // Default to >YES
        calStep = CAL_STEP_ADAPT_PROMPT;
      }
      watchdogSetLocation(WDG_LOC_MAIN_LOOP);
      break;
    }

    case CAL_STEP_ADAPT_PROMPT: {
      snprintf(line0, sizeof(line0), "Pt:%uW ADC:%4u", calTargetWatt, calSampledAvgAdc);
      if (calAdaptChoice == 0) {
        snprintf(line1, sizeof(line1), "Adapt all: >YES ");
      } else {
        snprintf(line1, sizeof(line1), "Adapt all: >NO  ");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        calAdaptChoice = (calAdaptChoice == 0) ? 1 : 0;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        if (calAdaptChoice == 0) {
          // Proportionally adapt all remaining points on the curve
          calAdaptAllPoints(ch, calTargetWatt, calSampledAvgAdc);
          calHasUnsavedChanges = true;
          hasUnsavedConfig = true;
          calStatusMsgLine0 = "All Pts Adapted!";
          calStatusMsgLine1 = "Stored in RAM   ";
          calStep = CAL_STEP_STATUS_MSG;
        } else {
          // Single point update: check monotonicity first
          uint8_t confIdx = 0;
          if (calCheckConflict(ch, calTargetWatt, calSampledAvgAdc, &confIdx)) {
            calConflictPointIdx = confIdx;
            calConflictChoice = 0;
            calStep = CAL_STEP_CONFLICT_WARN;
          } else {
            if (isAddingNewPoint) {
              calAddPoint(ch, calTargetWatt, calSampledAvgAdc);
            } else {
              calUpdatePoint(ch, calSelectedPoint, calTargetWatt, calSampledAvgAdc);
            }
            calHasUnsavedChanges = true;
            hasUnsavedConfig = true;
            calStep = CAL_STEP_SAMPLE_DONE;
          }
        }
      }
      break;
    }

    case CAL_STEP_CONFLICT_WARN: {
      CalPoint_t *cp = &activeCal.channels[ch].points[calConflictPointIdx];
      snprintf(line0, sizeof(line0), "Conflict:%uW=%u", cp->value, cp->raw);
      if (calConflictChoice == 0) {
        snprintf(line1, sizeof(line1), "Fix: >ADJUST    ");
      } else {
        snprintf(line1, sizeof(line1), "Fix: >DISCARD   ");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        calConflictChoice = (calConflictChoice == 0) ? 1 : 0;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        if (calConflictChoice == 0) {
          calAdaptAllPoints(ch, calTargetWatt, calSampledAvgAdc);
          calHasUnsavedChanges = true;
          hasUnsavedConfig = true;
          calStatusMsgLine0 = "Curve Corrected!";
          calStatusMsgLine1 = "Stored in RAM   ";
          calStep = CAL_STEP_STATUS_MSG;
        } else {
          calStatusMsgLine0 = "Sample Discarded";
          calStatusMsgLine1 = "Curve Unchanged ";
          calStep = CAL_STEP_STATUS_MSG;
        }
      }
      break;
    }

    case CAL_STEP_SAMPLE_DONE: {
      snprintf(line0, sizeof(line0), "Pt Done! (%uW)", calTargetWatt);
      snprintf(line1, sizeof(line1), "ADC:%4u (SEL:OK)", calSampledAvgAdc);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        buttonClearAll();
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_REMOVE_PICK: {
      uint8_t totalOpts = count + 2;
      if (calSelectedPoint < count) {
        CalPoint_t *p = &activeCal.channels[ch].points[calSelectedPoint];
        snprintf(line0, sizeof(line0), "Remove Pt %u/%u?", calSelectedPoint + 1, count);
        snprintf(line1, sizeof(line1), "ADC:%4u W:%4u", p->raw, p->value);
      } else if (calSelectedPoint == count) {
        snprintf(line0, sizeof(line0), "%s: Remove Pts", chName);
        snprintf(line1, sizeof(line1), "< Remove All >  ");
      } else {
        snprintf(line0, sizeof(line0), "%s: Remove Pts", chName);
        snprintf(line1, sizeof(line1), "< Back to Menu >");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calSelectedPoint = (calSelectedPoint + 1) % totalOpts;
      } else if (buttonJustPressed(15)) {
        calSelectedPoint = (calSelectedPoint + totalOpts - 1) % totalOpts;
      } else if (buttonLongHold(14, 500)) {
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        if (calSelectedPoint < count) {
          calStep = CAL_STEP_REMOVE_CONFIRM;
        } else if (calSelectedPoint == count) {
          calStep = CAL_STEP_REMOVE_ALL_CONFIRM;
        } else {
          calStep = CAL_STEP_MENU;
        }
      }
      break;
    }

    case CAL_STEP_REMOVE_CONFIRM: {
      CalPoint_t *p = &activeCal.channels[ch].points[calSelectedPoint];
      snprintf(line0, sizeof(line0), "Del %uW (ADC:%u)?", p->value, p->raw);
      snprintf(line1, sizeof(line1), "SEL:Yes  DN:No  ");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14)) {
        calRemovePoint(ch, calSelectedPoint);
        calHasUnsavedChanges = true;
        hasUnsavedConfig = true;
        calStatusMsgLine0 = "Point Removed!";
        calStatusMsgLine1 = "Updated in RAM  ";
        calStep = CAL_STEP_STATUS_MSG;
      } else if (buttonJustPressed(15) || buttonLongHold(14, 500)) {
        calStep = CAL_STEP_REMOVE_PICK;
      }
      break;
    }

    case CAL_STEP_REMOVE_ALL_CONFIRM: {
      snprintf(line0, sizeof(line0), "CLEAR ALL PTS?");
      snprintf(line1, sizeof(line1), "SEL:Yes  DN:No  ");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14)) {
        calRemoveAllPoints(ch);
        calHasUnsavedChanges = true;
        hasUnsavedConfig = true;
        calStatusMsgLine0 = "All Pts Cleared!";
        calStatusMsgLine1 = "Updated in RAM  ";
        calStep = CAL_STEP_STATUS_MSG;
      } else if (buttonJustPressed(15) || buttonLongHold(14, 500)) {
        calStep = CAL_STEP_REMOVE_PICK;
      }
      break;
    }

    case CAL_STEP_VIEW_POINTS: {
      if (count == 0) {
        lcdPrintRow(0, "No Points Found ");
        lcdPrintRow(1, "SEL: Back       ");
      } else {
        if (calViewPointIndex >= count) calViewPointIndex = 0;
        CalPoint_t *p = &activeCal.channels[ch].points[calViewPointIndex];
        snprintf(line0, sizeof(line0), "%s Pt %u of %u", chName, calViewPointIndex + 1, count);
        snprintf(line1, sizeof(line1), "ADC:%4u W:%4u", p->raw, p->value);
        lcdPrintRow(0, line0);
        lcdPrintRow(1, line1);
      }

      if (buttonJustPressed(13)) {
        if (count > 0) calViewPointIndex = (calViewPointIndex + 1) % count;
      } else if (buttonJustPressed(15)) {
        if (count > 0) calViewPointIndex = (calViewPointIndex + count - 1) % count;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_STATUS_MSG: {
      lcdPrintRow(0, calStatusMsgLine0);
      lcdPrintRow(1, calStatusMsgLine1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        buttonClearAll();
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_SAVE_PROMPT: {
      snprintf(line0, sizeof(line0), "Save config?");
      if (savePromptChoice == 0) {
        snprintf(line1, sizeof(line1), "Save: >YES      ");
      } else {
        snprintf(line1, sizeof(line1), "Save: >NO       ");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        savePromptChoice = (savePromptChoice == 0) ? 1 : 0;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        if (savePromptChoice == 0) {
          lcdPrintRow(0, "Saving Config...");
          lcdPrintRow(1, "Do Not Power Off");
          delay_ms(350);
          watchdogSetLocation(WDG_LOC_FLASH_WRITE);
          calSaveToFlash();
          watchdogSetLocation(WDG_LOC_MAIN_LOOP);
          calHasUnsavedChanges = false;
          hasUnsavedConfig = false;
        }
        MainMenuAction_t act = (ch == CAL_CH_FWD) ? MM_ACTION_CAL_FWD :
                               (ch == CAL_CH_REF) ? MM_ACTION_CAL_REF : MM_ACTION_CAL_RAD;
        buttonMenuIndex = getMainMenuIndexForAction(act, hasUnsavedConfig);
        currentMenu = MAIN_MENU;
        calStep = CAL_STEP_MENU;
      }
      break;
    }
  }
}
