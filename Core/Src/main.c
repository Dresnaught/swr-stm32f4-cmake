#include "lcd.h"
#include "adc.h"
#include "conversion.h"
#include "calibration.h"
#include "protection.h"
#include "button.h"
#include "menu.h"
#include "menu_main_screen.h"
#include "stm32f401xc.h"
#include "sytick.h"
#include "watchdog.h"
#include "buzzer.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

int main(void) {
  // Check RCC reset flags and crash breadcrumb before clearing
  watchdogCheckResetReason();

  timeInit();
  I2C1Init();
  lcd_init();
  buttonInit();
  buzzerInit();
  ADCInit();
  calInit(); // Load calibration & protection settings from Flash
  protectionInit(); // Initialize protection relay / optocoupler pin (PB2)
  updateRunningText(); // Pre-populate running marquee buffer

  // Configure PC13 (Status LED on BlackPill) as output
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->MODER &= ~(0x3 << (13 * 2));
  GPIOC->MODER |= (0x1 << (13 * 2));

  // If rebooted by watchdog, show the specific crash reason
  if (watchdogWasResetByIWDG()) {
    char reasonBuf[17];
    snprintf(reasonBuf, sizeof(reasonBuf), "Why: %-11s", watchdogGetCrashReasonStr());
    lcdCursor(0, 0);
    lcdString("*WATCHDOG RESET*");
    lcdCursor(1, 0);
    lcdString(reasonBuf);
    buzzerBeep(150);

    // Wait up to 2.5s or until user presses any button
    for (int i = 0; i < 25; i++) {
      buzzerUpdate();
      delay_ms(100);
      if (buttonJustPressed(13) || buttonJustPressed(14) || buttonJustPressed(15)) {
        break;
      }
    }
  } else {
    lcdCursor(0, 0);
    lcdString("SWR Power Meter ");
    lcdCursor(1, 0);
    lcdString("Digital STC8/F4 ");
    delay_ms(900);
  }

  // Initialize independent watchdog for crash and RF lockup recovery
  watchdogInit();

  while (1) {
    watchdogSetLocation(WDG_LOC_MAIN_LOOP);
    watchdogRefresh();
    buzzerUpdate();
    buttonUpdate();

    watchdogSetLocation(WDG_LOC_ADC_READ);
    readReading();

    watchdogSetLocation(WDG_LOC_MAIN_LOOP);
    protectionCheck(calibratedFWD, calibratedRAD, calculatedSWRValue);

    if (updateReadings()) {
      watchdogSetLocation(WDG_LOC_LCD_I2C);
      displayMenu(currentMenu);
    }
  }
}
