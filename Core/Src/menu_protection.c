#include "menu_protection.h"
#include "menu.h"
#include "lcd.h"
#include "button.h"
#include "sytick.h"
#include "protection.h"
#include "calibration.h"
#include "watchdog.h"
#include <stdio.h>

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

void protectionMenu(void) {
  char line0[32];
  char line1[32];
  uint16_t radLim = protectionGetRadLimit();
  uint16_t swrLim = protectionGetSwrLimit();

  switch (protStep) {
    case PROT_STEP_MENU: {
      char item0[20];
      char item1[20];
      char item2[20];
      const char *items[7];

      if (protectionIsTripped()) {
        snprintf(item0, sizeof(item0), "1.State: TRIPPED");
      } else {
        snprintf(item0, sizeof(item0), "1.Protect: %s", protectionIsEnabled() ? "ON" : "OFF");
      }
      if (radLim > 0) snprintf(item1, sizeof(item1), "2.RAD Lim: %2uW", radLim);
      else snprintf(item1, sizeof(item1), "2.RAD Lim: OFF");

      if (swrLim > 0) snprintf(item2, sizeof(item2), "3.SWR Lim:%u.%u", swrLim / 100, (swrLim % 100) / 10);
      else snprintf(item2, sizeof(item2), "3.SWR Lim: OFF");

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
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        switch (protMenuIndex) {
          case 0:
            protectionToggleEnabled();
            hasUnsavedConfig = true;
            break;
          case 1:
            protStep = PROT_STEP_SET_RAD;
            break;
          case 2:
            protStep = PROT_STEP_SET_SWR;
            break;
          case 3:
            protectionToggleRelay();
            break;
          case 4:
            protectionReset();
            protStatusLine0 = "Trip Reset OK";
            protStatusLine1 = "Relay Restored";
            protStep = PROT_STEP_STATUS;
            break;
          case 5:
            lcdPrintRow(0, "Saving Config...");
            lcdPrintRow(1, "Do Not Power Off");
            delay_ms(350);
            watchdogSetLocation(WDG_LOC_FLASH_WRITE);
            calSaveToFlash();
            watchdogSetLocation(WDG_LOC_MAIN_LOOP);
            hasUnsavedConfig = false;
            protStatusLine0 = "Flash Storage";
            protStatusLine1 = "Settings Saved!";
            protStep = PROT_STEP_STATUS;
            break;
          case 6:
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

      if (buttonRepeat(13)) {
        protectionSetRadLimit(stepRadLimitUp(radLim));
        hasUnsavedConfig = true;
      } else if (buttonRepeat(15)) {
        protectionSetRadLimit(stepRadLimitDown(radLim));
        hasUnsavedConfig = true;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
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

      if (buttonRepeat(13)) {
        protectionSetSwrLimit(stepSwrLimitUp(swrLim));
        hasUnsavedConfig = true;
      } else if (buttonRepeat(15)) {
        protectionSetSwrLimit(stepSwrLimitDown(swrLim));
        hasUnsavedConfig = true;
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
        protStep = PROT_STEP_MENU;
      }
      break;
    }

    case PROT_STEP_STATUS: {
      lcdPrintRow(0, protStatusLine0);
      lcdPrintRow(1, protStatusLine1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        buttonClearAll();
        protStep = PROT_STEP_MENU;
      }
      break;
    }
  }
}
