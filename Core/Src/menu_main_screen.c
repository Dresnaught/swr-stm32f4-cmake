#include "menu_main_screen.h"
#include "menu.h"
#include "lcd.h"
#include "button.h"
#include "sytick.h"
#include "protection.h"
#include "calibration.h"
#include <stdio.h>
#include <string.h>

static uint8_t mainScreenView = 0; // 0 = Bar/Primary, 1 = REF numeric, 2 = RAD numeric
static uint32_t lastActivityTime = 1;
static bool inScreenSaver = false;
static uint32_t lastScrollTime = 0;
static uint8_t runningTextScrollIdx = 0;
static char runningTextBuffer[128];
static uint8_t runningTextLen = 0;

void updateRunningText(void) {
  uint16_t maxFwd = activeCal.channels[CAL_CH_FWD].maxWatts;
  uint16_t maxRef = activeCal.channels[CAL_CH_REF].maxWatts;
  uint16_t maxRad = activeCal.channels[CAL_CH_RAD].maxWatts;
  const char *protStr = activeCal.protection.enabled ? "ACTIVE" : "OFF";

  snprintf(runningTextBuffer, sizeof(runningTextBuffer),
           "  *** DIGITAL SWR & PWR METER *** MAX FWD:%uW  REF:%uW  RAD:%uW *** PROTECTION:%s ***",
           maxFwd, maxRef, maxRad, protStr);
  runningTextLen = (uint8_t)strlen(runningTextBuffer);
}

void renderPowerBar(char *outBuf16, uint16_t currentW, uint16_t maxW, uint8_t style) {
  if (maxW == 0) maxW = 1;
  if (currentW > maxW) currentW = maxW;

  // 16 columns * 5 pixels per column = 80 pixels full scale
  uint8_t totalPixels = (uint8_t)(((uint32_t)currentW * 80U) / maxW);

  if (style == BAR_STYLE_SCALE) {
    if (totalPixels == 0) {
      for (uint8_t i = 0; i < 16; i++) outBuf16[i] = ' ';
      outBuf16[16] = '\0';
      return;
    }

    uint8_t tipCol = (totalPixels - 1) / 5;
    if (tipCol >= 16) tipCol = 15;

    for (uint8_t col = 0; col < 16; col++) {
      if (col < tipCol) {
        outBuf16[col] = (col % 2 == 0) ? '\x06' : '\x07';
      } else if (col == tipCol) {
        outBuf16[col] = '\x08'; // Custom scale character in CGRAM 0
      } else {
        outBuf16[col] = ' ';
      }
    }
    outBuf16[16] = '\0';
    return;
  }

  if (style == BAR_STYLE_PIPES) {
    // 5 vertical pipe bars per character cell
    for (uint8_t col = 0; col < 16; col++) {
      uint8_t colStart = col * 5;
      if (totalPixels >= colStart + 5) {
        outBuf16[col] = '\x05'; // Full 5 solid bars
      } else if (totalPixels > colStart) {
        uint8_t rem = totalPixels - colStart; // 1 to 4 bars
        outBuf16[col] = (char)rem;
      } else {
        outBuf16[col] = ' ';
      }
    }
    outBuf16[16] = '\0';
    return;
  }

  // BAR_STYLE_OFF: Blank bar
  for (uint8_t i = 0; i < 16; i++) outBuf16[i] = ' ';
  outBuf16[16] = '\0';
}

void renderMainScreen(void) {
  char buf0[32];
  char buf1[32];

  // 1. Protection Trip Screen
  if (protectionIsTripped()) {
    inScreenSaver = false;
    lastActivityTime = now;
    if (protectionGetTripCause() == TRIP_RAD_OVERPOWER) {
      lcdPrintRow(0, "*TRIP* HI RAD!  ");
    } else {
      lcdPrintRow(0, "*TRIP* HI SWR!  ");
    }
    lcdPrintRow(1, "SEL:Reset Hld:M ");

    if (buttonLongHold(14, 500)) {
      currentMenu = MAIN_MENU;
      buttonMenuIndex = 0;
    } else if (buttonShortRelease(14)) {
      protectionReset();
    } else if (buttonJustPressed(13) || buttonJustPressed(15)) {
      currentMenu = MAIN_MENU;
      buttonMenuIndex = 0;
    }
    return;
  }

  // 2. Activity Detection (buttons or transmission waking up screen saver)
  bool btn13 = buttonJustPressed(13);
  bool btn14 = buttonShortRelease(14);
  bool btn15 = buttonJustPressed(15);

  if (btn13 || btn14 || btn15) {
    lastActivityTime = now;
    if (inScreenSaver) {
      inScreenSaver = false;
      return; // Consume button press on wake-up
    }
  }

  // Reset inactivity timer when RF power is present (> 1W)
  if (lastCalibratedFWD > 1) {
    lastActivityTime = now;
    if (inScreenSaver) {
      inScreenSaver = false;
    }
  }

  // 3. Screen Saver Inactivity Trigger (30 seconds idle at 0W)
  if (!inScreenSaver && activeCal.protection.runningTextEnabled && lastActivityTime > 0 &&
      (now - lastActivityTime >= SCREEN_SAVER_TIMEOUT_MS)) {
    inScreenSaver = true;
    runningTextScrollIdx = 0;
    lastScrollTime = 0;
    updateRunningText();
  }

  // 4. Render Standby Marquee
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
    return;
  }

  // 5. Line 0: Forward Power & SWR readout
  uint16_t swr = lastCalculatedSWRValue;
  if (swr > 999) swr = 999;
  snprintf(buf0, sizeof(buf0), "FWD:%4uW S:%u.%02u", lastCalibratedFWD, swr / 100, swr % 100);
  lcdPrintRow(0, buf0);

  // 6. Line 1: Power bar or REF / RAD wattage
  uint8_t barStyle = activeCal.protection.barStyle;
  if (barStyle != BAR_STYLE_OFF && mainScreenView == 0) {
    renderPowerBar(buf1, lastCalibratedFWD, activeCal.channels[CAL_CH_FWD].maxWatts, barStyle);
  } else if (mainScreenView == 2 || (mainScreenView == 0 && UIState)) {
    snprintf(buf1, sizeof(buf1), "RAD: %3uW", lastCalibratedRAD);
  } else {
    snprintf(buf1, sizeof(buf1), "REF: %3uW", lastCalibratedREF);
  }
  lcdPrintRow(1, buf1);

  // 7. Button Handling
  if (btn14) {
    currentMenu = MAIN_MENU;
    buttonMenuIndex = 0;
  } else if (btn13) {
    if (barStyle != BAR_STYLE_OFF) {
      mainScreenView = (mainScreenView + 1) % 3;
    } else {
      UIState = !UIState;
    }
  } else if (btn15) {
    if (barStyle != BAR_STYLE_OFF) {
      mainScreenView = (mainScreenView + 2) % 3;
    } else {
      UIState = !UIState;
    }
  }
}
