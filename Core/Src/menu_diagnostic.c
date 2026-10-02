#include "menu_diagnostic.h"
#include "menu.h"
#include "lcd.h"
#include "button.h"
#include "adc.h"
#include "buzzer.h"
#include "protection.h"
#include "watchdog.h"
#include "stm32f401xc.h"
#include <stdio.h>

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
      } else if (buttonJustPressed(14)) {
        buttonClearAll();
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

      if (buttonJustPressed(14) || buttonJustPressed(15)) {
        buttonClearAll();
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
      snprintf(line0, sizeof(line0), "Boot: %s", watchdogGetResetReasonStr());
      if (watchdogWasResetByIWDG()) {
        snprintf(line1, sizeof(line1), "Why: %s", watchdogGetCrashReasonStr());
      } else {
        snprintf(line1, sizeof(line1), "Status: Normal  ");
      }
      lcdPrintRow(0, line0);
      lcdPrintRow(1, line1);

      if (buttonJustPressed(14) || buttonJustPressed(13) || buttonJustPressed(15)) {
        buttonClearAll();
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
