#include "menu.h"
#include "lcd.h"
#include "conversion.h"
#include "calibration.h"
#include "protection.h"
#include "button.h"
#include "sytick.h"
#include "adc.h"
#include "buzzer.h"
#include "watchdog.h"
#include "stm32f401xc.h"
#include <stdio.h>
#include <string.h>

MenuState currentMenu = MAIN_SCREEN;
bool UIState = false; // false = REF display, true = RAD (Radio-In) display

uint16_t lastCalibratedFWD = 0;
uint16_t lastCalibratedREF = 0;
uint16_t lastCalibratedRAD = 0;
uint16_t lastCalculatedSWRValue = 100;
float lastCalculatedSWRFloatValue = 1.0f;

static uint8_t buttonMenuIndex = 0;

// Calibration sub-state machine
typedef enum {
  CAL_STEP_MENU,                // Action selection
  CAL_STEP_PICK_POINT,          // Pick point to edit
  CAL_STEP_SET_WATT,            // Set target wattage
  CAL_STEP_PROMPT_RF,           // Ready prompt before sampling
  CAL_STEP_SAMPLING,            // Active 5-second sampling
  CAL_STEP_ADAPT_PROMPT,        // Prompt "Adapt all? >YES NO"
  CAL_STEP_CONFLICT_WARN,       // Monotonicity conflict warning
  CAL_STEP_SAMPLE_DONE,         // Show result after sampling
  CAL_STEP_REMOVE_PICK,         // Pick point to remove
  CAL_STEP_REMOVE_CONFIRM,      // Confirm single deletion
  CAL_STEP_REMOVE_ALL_CONFIRM,  // Confirm removing all points
  CAL_STEP_VIEW_POINTS,         // Browse active calibration points
  CAL_STEP_STATUS_MSG,          // Confirmation / alert message
  CAL_STEP_SAVE_PROMPT          // Prompt "Save config? >OK NO"
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
static bool calHasUnsavedChanges = false;
static uint8_t savePromptChoice = 0; // 0 = >OK  NO, 1 =  OK >NO
static bool hasUnsavedConfig = false;

// Calibration adapt and conflict choices
static uint8_t calAdaptChoice = 0;    // 0 = >YES NO, 1 = YES >NO
static uint8_t calConflictPointIdx = 0;
static uint8_t calConflictChoice = 0; // 0 = >ADJUST CANCEL, 1 = ADJUST >CANCEL

// Diagnostic sub-state machine
typedef enum {
  DIAG_STEP_MENU,
  DIAG_STEP_ADC_LIVE,
  DIAG_STEP_BUTTONS_LIVE,
  DIAG_STEP_RELAY,
  DIAG_STEP_BUZZER,
  DIAG_STEP_BOOT_REASON,
  DIAG_STEP_WATCHDOG_TEST
} DiagStepState;

static DiagStepState diagStep = DIAG_STEP_MENU;
static uint8_t diagMenuIndex = 0;

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

static uint8_t getMainMenuIndexForAction(MainMenuAction_t act, bool hasUnsaved) {
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

// Screen saver / running text state
static char runningTextBuffer[160];
static size_t runningTextLen = 0;
static uint16_t runningTextScrollIdx = 0;
static uint32_t lastScrollTime = 0;
static uint32_t lastActivityTime = 0;
static bool inScreenSaver = false;

void updateRunningText(void) {
  uint16_t fwdMax = calGetChannelMaxWatts(CAL_CH_FWD);
  uint16_t refMax = fwdMax / 10;
  uint16_t radMax = calGetChannelMaxWatts(CAL_CH_RAD);

  snprintf(runningTextBuffer, sizeof(runningTextBuffer),
           "  *** DIGITAL SWR & PWR METER *** MAX FWD:%uW  REF:%uW  RAD:%uW *** PROTECTION:%s ***  ",
           fwdMax, refMax, radMax,
           protectionIsEnabled() ? "ON" : "OFF");
  runningTextLen = strlen(runningTextBuffer);
  if (runningTextLen == 0) {
    runningTextBuffer[0] = ' ';
    runningTextBuffer[1] = '\0';
    runningTextLen = 1;
  }
}

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

// Protection sub-state machine
typedef enum {
  PROT_STEP_MENU,
  PROT_STEP_SET_RAD,
  PROT_STEP_SET_SWR,
  PROT_STEP_STATUS
} ProtStepState;

static ProtStepState protStep = PROT_STEP_MENU;
static uint8_t protMenuIndex = 0;
static const char *protStatusLine0 = "";
static const char *protStatusLine1 = "";

static uint8_t mainScreenView = 0; // 0 = default (Bar or REF), 1 = REF, 2 = RAD

static const char *barStyleNames[BAR_STYLE_COUNT] = {
  "1.Bar: Off    ",
  "2.Bar: Scale  ",
  "3.Bar: Pipes  "
};

static void renderPowerBar(char *outBuf16, uint16_t currentWatts, uint16_t maxWatts, uint8_t style) {
  if (maxWatts == 0) maxWatts = 1000;
  if (currentWatts > maxWatts) currentWatts = maxWatts;

  // 1. If FWD is 0: show nothing on cursor 1,0 (all spaces)
  if (currentWatts == 0) {
    for (uint8_t i = 0; i < 16; i++) {
      outBuf16[i] = ' ';
    }
    outBuf16[16] = '\0';
    return;
  }

  // Calculate pixel count across 80 horizontal dots (16 chars * 5 dots)
  uint8_t totalPixels = (uint8_t)(((uint32_t)currentWatts * 80U + (maxWatts / 2)) / maxWatts);
  if (totalPixels == 0) totalPixels = 1;
  if (totalPixels > 80) totalPixels = 80;

  if (style == BAR_STYLE_SCALE) {
    // 80-pixel scale bar:
    // Every 10 pixels is: 4 ticks (with 1-pixel uniform spacing) and a line
    // Even cell (pixels 0..4): ticks at sub-columns 1, 3
    // Odd cell (pixels 5..9): ticks at sub-columns 0, 2; half-line at sub-column 4
    // At the tip (current reading position), the line is ALWAYS full-height (<-full / <-long)
    // Intermediate lines are half-height (<-half)
    // Beyond the tip: empty spaces
    uint8_t tipPixel = totalPixels - 1; // 0..79
    uint8_t tipCol = tipPixel / 5;      // 0..15 (character index)
    uint8_t tipSubCol = tipPixel % 5;   // 0..4  (sub-column dot within cell)
    bool isOdd = (tipCol % 2 != 0);

    // Build dynamic tip pattern for CGRAM location 0
    uint8_t tipPattern[8] = {0};

    // Sub-columns before the tip in this cell
    for (uint8_t s = 0; s < tipSubCol; s++) {
      if (!isOdd) {
        // Even cell: ticks at sub-columns 1, 3 (2 pixels high at top)
        if (s == 1 || s == 3) {
          tipPattern[0] |= (1 << (4 - s));
          tipPattern[1] |= (1 << (4 - s));
        }
      } else {
        // Odd cell: ticks at sub-columns 0, 2; half-line at sub-column 4
        if (s == 0 || s == 2) {
          tipPattern[0] |= (1 << (4 - s));
          tipPattern[1] |= (1 << (4 - s));
        } else if (s == 4) {
          for (uint8_t r = 0; r < 5; r++) {
            tipPattern[r] |= (1 << (4 - s));
          }
        }
      }
    }

    // At the tip itself: ALWAYS a full-height line!
    for (uint8_t r = 0; r < 8; r++) {
      tipPattern[r] |= (1 << (4 - tipSubCol));
    }

    // Only update CGRAM if the tip pattern actually changed
    static uint8_t lastTipPattern[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (memcmp(lastTipPattern, tipPattern, 8) != 0) {
      memcpy(lastTipPattern, tipPattern, 8);
      lcdCreateChar(0, tipPattern);
    }

    for (uint8_t col = 0; col < 16; col++) {
      if (col < tipCol) {
        outBuf16[col] = (col % 2 == 0) ? '\x06' : '\x07';
      } else if (col == tipCol) {
        outBuf16[col] = '\x08'; // CGRAM location 0
      } else {
        outBuf16[col] = ' ';
      }
    }
    outBuf16[16] = '\0';
    return;
  }

  if (style == BAR_STYLE_PIPES) {
    // Continuous per-pixel bar using 5 vertical lines: |||||
    for (uint8_t col = 0; col < 16; col++) {
      uint8_t colStart = col * 5;
      if (totalPixels >= colStart + 5) {
        outBuf16[col] = '\x05'; // Full 5 lines (|||||)
      } else if (totalPixels > colStart) {
        uint8_t rem = totalPixels - colStart; // 1 to 4 lines
        outBuf16[col] = (char)rem; // Char 1..4 (\x01..\x04)
      } else {
        outBuf16[col] = ' ';
      }
    }
    outBuf16[16] = '\0';
    return;
  }

  // BAR_STYLE_OFF: empty
  for (uint8_t i = 0; i < 16; i++) {
    outBuf16[i] = ' ';
  }
  outBuf16[16] = '\0';
}

// Helper to write a padded 16-character line to the 1602 LCD
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

// 2-Row Compact Menu Renderer for 1602 LCD
static void lcdRender2RowMenu(const char *items[], uint8_t count, uint8_t selectedIndex) {
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

bool updateReadings(void) {
  if (currentMenu != MAIN_SCREEN) {
    lastActivityTime = now;
    return true;
  }

  // Fast-track user button interactions for immediate UI response
  if (buttonRead(13) || buttonRead(14) || buttonRead(15)) {
    return true;
  }

  // Fast-track protection trip status changes
  static bool lastTripped = false;
  bool nowTripped = protectionIsTripped();
  if (nowTripped != lastTripped) {
    lastTripped = nowTripped;
    lastActivityTime = now;
    if (inScreenSaver) {
      inScreenSaver = false;
    }
    return true;
  }

  // Active RF power check:
  // If user is transmitting (FWD > 1W or RAD > 1W), update activity time and wake up from standby
  if (calibratedFWD > 1 || calibratedRAD > 1) {
    lastActivityTime = now;
    if (inScreenSaver) {
      inScreenSaver = false;
      return true;
    }
  }

  // Live RF power changes with smooth 180ms throttle (~5.5 Hz LCD refresh)
  // Perfectly matched to HD44780 LCD response time for crystal-clear, readable digits without jitter
  if (!inScreenSaver) {
    if (calibratedFWD != lastCalibratedFWD || 
        calibratedREF != lastCalibratedREF || 
        calibratedRAD != lastCalibratedRAD) {
      static uint32_t lastPowerUpdate = 0;
      if ((now - lastPowerUpdate) >= 180) {
        lastPowerUpdate = now;
        lastCalibratedFWD = calibratedFWD;
        lastCalibratedREF = calibratedREF;
        lastCalibratedRAD = calibratedRAD;
        lastCalculatedSWRValue = calculatedSWRValue;
        lastCalculatedSWRFloatValue = calculatedSWRFloatValue;
        return true;
      }
    }
  }

  // Screen saver marquee scrolling refresh
  if (inScreenSaver) {
    if ((now - lastScrollTime) >= SCREEN_SAVER_SCROLL_MS) {
      return true;
    }
    return false;
  }

  // Screen saver inactivity timer trigger (30 seconds)
  if (lastActivityTime == 0) lastActivityTime = now;
  if (activeCal.protection.runningTextEnabled && (now - lastActivityTime >= SCREEN_SAVER_TIMEOUT_MS)) {
    return true;
  }

  // Periodic main screen refresh (180ms = ~5.5 Hz)
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
      lcdRender2RowMenu(calActionItems, CAL_ACTION_COUNT, calMenuActionIndex);

      if (buttonJustPressed(13)) {
        calMenuActionIndex = (calMenuActionIndex + 1) % CAL_ACTION_COUNT;
      } else if (buttonJustPressed(15)) {
        calMenuActionIndex = (calMenuActionIndex + CAL_ACTION_COUNT - 1) % CAL_ACTION_COUNT;
      } else if (buttonShortRelease(14)) {
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
            if (calSaveToFlash()) {
              calHasUnsavedChanges = false;
              hasUnsavedConfig = false;
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
            calHasUnsavedChanges = true;
            hasUnsavedConfig = true;
            calStatusMsgLine0 = "Factory Reset";
            calStatusMsgLine1 = "Defaults Loaded!";
            calStep = CAL_STEP_STATUS_MSG;
            break;

          case 6: // 7. Back
            if (calHasUnsavedChanges) {
              savePromptChoice = 0; // Default to >OK  NO
              calStep = CAL_STEP_SAVE_PROMPT;
            } else {
              MainMenuAction_t act = (ch == CAL_CH_FWD) ? MM_ACTION_CAL_FWD :
                                     (ch == CAL_CH_REF) ? MM_ACTION_CAL_REF : MM_ACTION_CAL_RAD;
              buttonMenuIndex = getMainMenuIndexForAction(act, hasUnsavedConfig);
              currentMenu = MAIN_MENU;
              calStep = CAL_STEP_MENU;
            }
            break;
        }
      }
      break;
    }

    case CAL_STEP_PICK_POINT: {
      snprintf(line0, sizeof(line0), "%s: Edit Point", chName);
      if (calSelectedPoint < count) {
        snprintf(line1, sizeof(line1), "Pt %u/%u: %4uW", 
                 calSelectedPoint + 1, count, 
                 activeCal.channels[ch].points[calSelectedPoint].value);
      } else {
        snprintf(line1, sizeof(line1), "< Back to Menu >");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        calSelectedPoint = (calSelectedPoint + 1) % (count + 1);
      } else if (buttonJustPressed(15)) {
        calSelectedPoint = (calSelectedPoint + count) % (count + 1);
      } else if (buttonLongHold(14, 500)) { // Hold SELECT to go back immediately
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        if (calSelectedPoint == count) {
          calStep = CAL_STEP_MENU;
        } else {
          calTargetWatt = activeCal.channels[ch].points[calSelectedPoint].value;
          if (calTargetWatt == 0 && calSelectedPoint > 0) {
            calTargetWatt = getSmartWattStep(maxW, 0);
          }
          calStep = CAL_STEP_SET_WATT;
        }
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
      snprintf(line1, sizeof(line1), "[%4uW] Hold:Bk", calTargetWatt);
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
      } else if (buttonLongHold(14, 500)) { // Hold SELECT to go back immediately!
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) { // Short release confirms watt!
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
        calStep = CAL_STEP_MENU; // Cancel / Back
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
        // Sampling complete!
        calSampledAvgAdc = (calAdcSampleCount > 0) ? (uint16_t)(calAdcSum / calAdcSampleCount) : 0;
        calAdaptChoice = 0; // Default to >YES
        calStep = CAL_STEP_ADAPT_PROMPT;
      }
      break;
    }

    case CAL_STEP_ADAPT_PROMPT: {
      snprintf(line0, sizeof(line0), "Pt:%uW ADC:%4u", calTargetWatt, calSampledAvgAdc);
      if (calAdaptChoice == 0) {
        snprintf(line1, sizeof(line1), "Adapt all? >YES NO");
      } else {
        snprintf(line1, sizeof(line1), "Adapt all?  YES >NO");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        calAdaptChoice = (calAdaptChoice == 0) ? 1 : 0;
      } else if (buttonShortRelease(14)) {
        if (calAdaptChoice == 0) {
          // YES: Adapt all existing points in this channel proportionally
          calAdaptAllPoints(ch, calTargetWatt, calSampledAvgAdc);
          calHasUnsavedChanges = true;
          hasUnsavedConfig = true;
          calStatusMsgLine0 = "All Pts Adapted!";
          calStatusMsgLine1 = "Stored in RAM   ";
          calStep = CAL_STEP_STATUS_MSG;
        } else {
          // NO: User chooses to keep only this single point
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
        snprintf(line1, sizeof(line1), "Fix? >ADJUST CANCL");
      } else {
        snprintf(line1, sizeof(line1), "Fix?  ADJUST >CANCL");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        calConflictChoice = (calConflictChoice == 0) ? 1 : 0;
      } else if (buttonShortRelease(14)) {
        if (calConflictChoice == 0) {
          // ADJUST: adapt curve to preserve strict monotonicity
          calAdaptAllPoints(ch, calTargetWatt, calSampledAvgAdc);
          calHasUnsavedChanges = true;
          hasUnsavedConfig = true;
          calStatusMsgLine0 = "Curve Corrected!";
          calStatusMsgLine1 = "Stored in RAM   ";
          calStep = CAL_STEP_STATUS_MSG;
        } else {
          // CANCEL: Discard sample, preserve intact curve
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

      if (buttonShortRelease(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU; // Goes directly back to that calibration menu!
      }
      break;
    }

    case CAL_STEP_REMOVE_PICK: {
      uint8_t totalOpts = count + 2; // points + <Remove All> + <Back>
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
      } else if (buttonLongHold(14, 500)) { // Hold SELECT to go back immediately
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        if (calSelectedPoint == count + 1) {
          calStep = CAL_STEP_MENU;
        } else if (calSelectedPoint == count) {
          calStep = CAL_STEP_REMOVE_ALL_CONFIRM;
        } else {
          calStep = CAL_STEP_REMOVE_CONFIRM;
        }
      }
      break;
    }

    case CAL_STEP_REMOVE_CONFIRM: {
      snprintf(line0, sizeof(line0), "Delete Pt %u/%u?", calSelectedPoint + 1, count);
      snprintf(line1, sizeof(line1), "SEL:Del  DN:Back");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonLongHold(14, 500) || buttonJustPressed(15)) { // Cancel
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) { // Confirm Delete
        calRemovePoint(ch, calSelectedPoint);
        calHasUnsavedChanges = true;
        hasUnsavedConfig = true;
        calStatusMsgLine0 = "Point Removed!";
        calStatusMsgLine1 = "Table Updated";
        calStep = CAL_STEP_STATUS_MSG;
      }
      break;
    }

    case CAL_STEP_REMOVE_ALL_CONFIRM: {
      snprintf(line0, sizeof(line0), "Clear ALL Pts?");
      snprintf(line1, sizeof(line1), "SEL:Yes  DN:Back");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonLongHold(14, 500) || buttonJustPressed(15)) { // Cancel
        calStep = CAL_STEP_MENU;
      } else if (buttonShortRelease(14)) {
        calRemoveAllPoints(ch);
        calHasUnsavedChanges = true;
        hasUnsavedConfig = true;
        calStatusMsgLine0 = "All Pts Cleared";
        calStatusMsgLine1 = "Not saved to ROM";
        calStep = CAL_STEP_STATUS_MSG;
      }
      break;
    }

    case CAL_STEP_VIEW_POINTS: {
      if (count == 0) {
        snprintf(line0, sizeof(line0), "%s Calibration", chName);
        snprintf(line1, sizeof(line1), "No Active Pts!");
      } else {
        CalPoint_t *p = &activeCal.channels[ch].points[calViewPointIndex];
        snprintf(line0, sizeof(line0), "%s Pt %u/%u View", chName, calViewPointIndex + 1, count);
        snprintf(line1, sizeof(line1), "Raw:%4u W:%4u", p->raw, p->value);
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(13)) {
        if (count > 0) calViewPointIndex = (calViewPointIndex + 1) % count;
      } else if (buttonJustPressed(15)) {
        if (count > 0) calViewPointIndex = (calViewPointIndex + count - 1) % count;
      } else if (buttonLongHold(14, 500) || buttonShortRelease(14)) {
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_SAVE_PROMPT: {
      lcdPrintRow(0, "Save config?    ");
      if (savePromptChoice == 0) {
        lcdPrintRow(1, ">OK  NO         ");
      } else {
        lcdPrintRow(1, " OK >NO         ");
      }

      if (buttonJustPressed(13) || buttonJustPressed(15)) {
        savePromptChoice = !savePromptChoice;
      } else if (buttonShortRelease(14)) {
        if (savePromptChoice == 0) {
          lcdPrintRow(0, "Saving Config...");
          lcdPrintRow(1, "Do Not Power Off");
          delay_ms(350);
          calSaveToFlash(); // Save to Flash!
          hasUnsavedConfig = false;
        } else {
          // Keep changes in RAM for this session, flag for Main Menu save option
          hasUnsavedConfig = true;
        }
        calHasUnsavedChanges = false;
        MainMenuAction_t act = (ch == CAL_CH_FWD) ? MM_ACTION_CAL_FWD :
                               (ch == CAL_CH_REF) ? MM_ACTION_CAL_REF : MM_ACTION_CAL_RAD;
        buttonMenuIndex = getMainMenuIndexForAction(act, hasUnsavedConfig);
        currentMenu = MAIN_MENU;
        calStep = CAL_STEP_MENU;
      }
      break;
    }

    case CAL_STEP_STATUS_MSG: {
      lcdPrintRow(0, calStatusMsgLine0);
      lcdPrintRow(1, calStatusMsgLine1);

      if (buttonShortRelease(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        calStep = CAL_STEP_MENU;
      }
      break;
    }
  }
}

void protectionMenu(void) {
  char line0[32];
  char line1[32];
  uint16_t radLim = protectionGetRadLimit();
  uint16_t swrLim = protectionGetSwrLimit();
  bool enabled = protectionIsEnabled();

  switch (protStep) {
    case PROT_STEP_MENU: {
      char item0[18];
      char item1[18];
      char item2[18];
      const char *items[7];

      snprintf(item0, sizeof(item0), "1.Prot: %s", enabled ? "ON" : "OFF");
      if (radLim > 0) {
        snprintf(item1, sizeof(item1), "2.RAD: %2uW", radLim);
      } else {
        snprintf(item1, sizeof(item1), "2.RAD: OFF");
      }
      if (swrLim > 0) {
        snprintf(item2, sizeof(item2), "3.SWR: %u.%u", swrLim / 100, (swrLim % 100) / 10);
      } else {
        snprintf(item2, sizeof(item2), "3.SWR: OFF");
      }

      items[0] = item0;
      items[1] = item1;
      items[2] = item2;
      items[3] = "4.Relay Test";
      items[4] = "5.Reset Trip";
      items[5] = "6.Save Flash";
      items[6] = "7.Back";

      lcdRender2RowMenu(items, 7, protMenuIndex);

      if (buttonJustPressed(13)) {
        protMenuIndex = (protMenuIndex + 1) % 7;
      } else if (buttonJustPressed(15)) {
        protMenuIndex = (protMenuIndex + 6) % 7;
      } else if (buttonShortRelease(14)) {
        switch (protMenuIndex) {
          case 0: // Toggle ON / OFF
            protectionToggleEnabled();
            hasUnsavedConfig = true;
            break;
          case 1: // Set RAD limit
            protStep = PROT_STEP_SET_RAD;
            break;
          case 2: // Set SWR limit
            protStep = PROT_STEP_SET_SWR;
            break;
          case 3: // Relay Test
            protectionToggleRelay();
            break;
          case 4: // Reset Trip
            protectionReset();
            protStatusLine0 = "Trip Reset OK";
            protStatusLine1 = "Relay Restored";
            protStep = PROT_STEP_STATUS;
            break;
          case 5: // Save Settings
            lcdPrintRow(0, "Saving Config...");
            lcdPrintRow(1, "Do Not Power Off");
            delay_ms(350);
            calSaveToFlash();
            hasUnsavedConfig = false;
            protStatusLine0 = "Flash Storage";
            protStatusLine1 = "Settings Saved!";
            protStep = PROT_STEP_STATUS;
            break;
          case 6: // Back
            buttonMenuIndex = getMainMenuIndexForAction(MM_ACTION_PROTECTIONS, hasUnsavedConfig);
            currentMenu = MAIN_MENU;
            break;
        }
      }
      break;
    }

    case PROT_STEP_SET_RAD: {
      snprintf(line0, sizeof(line0), "RAD Trip Limit");
      if (radLim > 0) {
        snprintf(line1, sizeof(line1), "Limit: [ %2u ] W", radLim);
      } else {
        snprintf(line1, sizeof(line1), "Limit: [ OFF ]");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonRepeat(13)) { // UP
        protectionSetRadLimit(stepRadLimitUp(radLim));
        hasUnsavedConfig = true;
      } else if (buttonRepeat(15)) { // DOWN
        protectionSetRadLimit(stepRadLimitDown(radLim));
        hasUnsavedConfig = true;
      } else if (buttonLongHold(14, 500) || buttonShortRelease(14)) { // SELECT
        protStep = PROT_STEP_MENU;
      }
      break;
    }

    case PROT_STEP_SET_SWR: {
      snprintf(line0, sizeof(line0), "SWR Trip Limit");
      if (swrLim > 0) {
        snprintf(line1, sizeof(line1), "Limit: [ %u.%u ]", swrLim / 100, (swrLim % 100) / 10);
      } else {
        snprintf(line1, sizeof(line1), "Limit: [ OFF ]");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonRepeat(13)) { // UP
        protectionSetSwrLimit(stepSwrLimitUp(swrLim));
        hasUnsavedConfig = true;
      } else if (buttonRepeat(15)) { // DOWN
        protectionSetSwrLimit(stepSwrLimitDown(swrLim));
        hasUnsavedConfig = true;
      } else if (buttonLongHold(14, 500) || buttonShortRelease(14)) { // SELECT
        protStep = PROT_STEP_MENU;
      }
      break;
    }

    case PROT_STEP_STATUS: {
      lcdPrintRow(0, protStatusLine0);
      lcdPrintRow(1, protStatusLine1);

      if (buttonShortRelease(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        protStep = PROT_STEP_MENU;
      }
      break;
    }
  }
}

static const char *diagItems[] = {
  "1.Live ADC",
  "2.Buttons Test",
  "3.Relay Test",
  "4.Buzzer Test",
  "5.Boot Reason",
  "6.Watchdog Test",
  "7.Back"
};
#define DIAG_ITEM_COUNT 7

void diagnosticMenu(void) {
  char line0[32];
  char line1[32];

  switch (diagStep) {
    case DIAG_STEP_MENU: {
      lcdRender2RowMenu(diagItems, DIAG_ITEM_COUNT, diagMenuIndex);

      if (buttonJustPressed(13)) {
        diagMenuIndex = (diagMenuIndex + 1) % DIAG_ITEM_COUNT;
      } else if (buttonJustPressed(15)) {
        diagMenuIndex = (diagMenuIndex + DIAG_ITEM_COUNT - 1) % DIAG_ITEM_COUNT;
      } else if (buttonShortRelease(14)) {
        switch (diagMenuIndex) {
          case 0: diagStep = DIAG_STEP_ADC_LIVE; break;
          case 1: diagStep = DIAG_STEP_BUTTONS_LIVE; break;
          case 2: diagStep = DIAG_STEP_RELAY; break;
          case 3: diagStep = DIAG_STEP_BUZZER; break;
          case 4: diagStep = DIAG_STEP_BOOT_REASON; break;
          case 5: diagStep = DIAG_STEP_WATCHDOG_TEST; break;
          case 6: // Back
            buttonMenuIndex = getMainMenuIndexForAction(MM_ACTION_DIAGNOSTIC, hasUnsavedConfig);
            currentMenu = MAIN_MENU;
            break;
        }
      }
      break;
    }

    case DIAG_STEP_ADC_LIVE: {
      uint16_t rawF = 0, rawR = 0, rawRad = 0;
      adcCH0Raw(&rawF);
      adcCH1Raw(&rawR);
      adcCH2Raw(&rawRad);

      snprintf(line0, sizeof(line0), "F:%4u R:%4u", rawF, rawR);
      snprintf(line1, sizeof(line1), "RAD:%4u (SEL:Ex)", rawRad);
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14) || buttonLongHold(14, 500) || buttonJustPressed(15)) {
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }

    case DIAG_STEP_BUTTONS_LIVE: {
      // Direct register read of GPIO inputs: PB13 (UP), PB14 (SEL), PB15 (DN)
      bool upPressed  = !(GPIOB->IDR & (1U << 13));
      bool selPressed = !(GPIOB->IDR & (1U << 14));
      bool dnPressed  = !(GPIOB->IDR & (1U << 15));

      snprintf(line0, sizeof(line0), "UP:%s SEL:%s", upPressed ? "PR " : "REL", selPressed ? "PR " : "REL");
      snprintf(line1, sizeof(line1), "DN:%s (Hld SEL)", dnPressed ? "PR " : "REL");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonLongHold(14, 500)) {
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }

    case DIAG_STEP_RELAY: {
      bool isNorm = (GPIOB->ODR & (1U << 2)) ? true : false;
      snprintf(line0, sizeof(line0), "Relay Pin PB2");
      snprintf(line1, sizeof(line1), "State: [ %s ]", isNorm ? "NORMAL" : "TRIPPED");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14)) {
        protectionToggleRelay();
      } else if (buttonLongHold(14, 500) || buttonJustPressed(13) || buttonJustPressed(15)) {
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }

    case DIAG_STEP_BUZZER: {
      bool bzOn = buzzerGet();
      snprintf(line0, sizeof(line0), "Buzzer Pin PB0");
      snprintf(line1, sizeof(line1), "State:[ %s ] SEL", bzOn ? "ON " : "OFF");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14)) {
        buzzerSet(!bzOn);
      } else if (buttonLongHold(14, 500) || buttonJustPressed(13) || buttonJustPressed(15)) {
        buzzerSet(false);
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }

    case DIAG_STEP_BOOT_REASON: {
      snprintf(line0, sizeof(line0), "Boot Reason:");
      snprintf(line1, sizeof(line1), "%s", watchdogGetResetReasonStr());
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }

    case DIAG_STEP_WATCHDOG_TEST: {
      snprintf(line0, sizeof(line0), "Test IWDG Halt?");
      snprintf(line1, sizeof(line1), "SEL:Freeze DN:Ex");
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonShortRelease(14)) {
        lcdPrintRow(0, "Halting CPU...");
        lcdPrintRow(1, "IWDG Will Reset!");
        watchdogTriggerResetTest();
      } else if (buttonLongHold(14, 500) || buttonJustPressed(15)) {
        diagStep = DIAG_STEP_MENU;
      }
      break;
    }
  }
}

void displayMenu(MenuState menu) {
  char buf0[32];
  char buf1[32];

  switch (menu) {
    case MAIN_SCREEN: {
      // Check if protection is tripped
      if (protectionIsTripped()) {
        inScreenSaver = false;
        lastActivityTime = now;
        if (protectionGetTripCause() == TRIP_RAD_OVERPOWER) {
          lcdPrintRow(0, "*TRIP* HI RAD!  ");
        } else {
          lcdPrintRow(0, "*TRIP* HI SWR!  ");
        }
        lcdPrintRow(1, "SEL:Reset Hld:M ");

        if (buttonLongHold(14, 500)) { // Hold SELECT goes directly to Main Menu
          currentMenu = MAIN_MENU;
          buttonMenuIndex = 0;
        } else if (buttonShortRelease(14)) { // Short press resets relay trip
          protectionReset();
        } else if (buttonJustPressed(13) || buttonJustPressed(15)) {
          currentMenu = MAIN_MENU;
          buttonMenuIndex = 0;
        }
        break;
      }

      // Check user interaction or power changes to exit screensaver
      bool btn13 = buttonJustPressed(13);
      bool btn14 = buttonShortRelease(14);
      bool btn15 = buttonJustPressed(15);

      if (btn13 || btn14 || btn15) {
        lastActivityTime = now;
        if (inScreenSaver) {
          inScreenSaver = false;
          // Consume button press on waking up
          break;
        }
      }

      // Check inactivity to enter screen saver (30 seconds)
      if (!inScreenSaver && activeCal.protection.runningTextEnabled && lastActivityTime > 0 && 
          (now - lastActivityTime >= SCREEN_SAVER_TIMEOUT_MS)) {
        inScreenSaver = true;
        runningTextScrollIdx = 0;
        lastScrollTime = 0; // Trigger immediate scroll render
        updateRunningText();
      }

      // If Screen Saver is active
      if (inScreenSaver) {
        lcdPrintRow(0, SCREEN_SAVER_ROW0_TEXT);
        if (runningTextLen > 0 && (now - lastScrollTime >= SCREEN_SAVER_SCROLL_MS)) {
          lastScrollTime = now;
          char scrollBuf[17];
          for (int i = 0; i < 16; i++) {
            scrollBuf[i] = runningTextBuffer[(runningTextScrollIdx + i) % runningTextLen];
          }
          scrollBuf[16] = '\0';
          lcdPrintRow(1, scrollBuf);
          runningTextScrollIdx = (runningTextScrollIdx + 1) % runningTextLen;
        }
        break;
      }

      // Row 0: FWD Power (0-1000W) & SWR (1.00-9.99), formatted strictly <= 16 chars using pure integer math
      uint16_t swr = lastCalculatedSWRValue;
      if (swr > 999) swr = 999;
      snprintf(buf0, sizeof(buf0), "FWD:%4uW S:%u.%02u", lastCalibratedFWD, swr / 100, swr % 100);
      lcdPrintRow(0, buf0);

      // Row 1: Power Bar or clean REF/RAD numerical readout (no (Refl) or (R-In))
      uint8_t barStyle = activeCal.protection.barStyle;
      if (barStyle != BAR_STYLE_OFF && mainScreenView == 0) {
        // Power Bar mode at cursor (1, 0)
        renderPowerBar(buf1, lastCalibratedFWD, activeCal.channels[CAL_CH_FWD].maxWatts, barStyle);
      } else if (mainScreenView == 2 || (mainScreenView == 0 && UIState)) {
        snprintf(buf1, sizeof(buf1), "RAD: %3uW", lastCalibratedRAD);
      } else {
        snprintf(buf1, sizeof(buf1), "REF: %3uW", lastCalibratedREF);
      }
      lcdPrintRow(1, buf1);

      if (btn14) { // SELECT opens main menu
        currentMenu = MAIN_MENU;
        buttonMenuIndex = 0;
      } else if (btn13) { // UP button
        if (barStyle != BAR_STYLE_OFF) {
          mainScreenView = (mainScreenView + 1) % 3;
        } else {
          UIState = !UIState;
        }
      } else if (btn15) { // DOWN button
        if (barStyle != BAR_STYLE_OFF) {
          mainScreenView = (mainScreenView + 2) % 3;
        } else {
          UIState = !UIState;
        }
      }
      break;
    }

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
            calSaveToFlash();
            hasUnsavedConfig = false;
            buttonMenuIndex = 0;
            lcdPrintRow(0, "Config Saved!   ");
            lcdPrintRow(1, "Stored in Flash ");
            delay_ms(800);
            break;
          case MM_ACTION_CAL_FWD:
            currentMenu = MAIN_CAL_FWD_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            calHasUnsavedChanges = false;
            break;
          case MM_ACTION_CAL_REF:
            currentMenu = MAIN_CAL_REF_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            calHasUnsavedChanges = false;
            break;
          case MM_ACTION_CAL_RAD:
            currentMenu = MAIN_CAL_RAD_MENU;
            calStep = CAL_STEP_MENU;
            calMenuActionIndex = 0;
            calHasUnsavedChanges = false;
            break;
          case MM_ACTION_PROTECTIONS:
            currentMenu = MAIN_PROTECTION_MENU;
            protStep = PROT_STEP_MENU;
            protMenuIndex = 0;
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
            diagStep = DIAG_STEP_MENU;
            diagMenuIndex = 0;
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
        // Hold SELECT to return to Main Menu, cursor placed directly on Display Mode
        mainScreenView = 0; // Return to default view on main screen
        buttonMenuIndex = getMainMenuIndexForAction(MM_ACTION_DISPLAY_MODE, hasUnsavedConfig);
        currentMenu = MAIN_MENU;
      } else if (buttonShortRelease(14)) {
        // Confirm selection, stay in Display Mode!
        lcdPrintRow(0, "Power Bar Mode:");
        lcdPrintRow(1, "Selected! (OK)  ");
        delay_ms(500);
      }
      break;
    }
  }
}
