#include "lcd.h"
#include "adc.h"
#include "conversion.h"
#include "calibration.h"
#include "protection.h"
#include "button.h"
#include "menu.h"
#include "stm32f401xc.h"
#include "sytick.h"
#include "watchdog.h"
#include "buzzer.h"
#include <stdint.h>
#include <stdbool.h>

int main(void) {
  // 1. Inspect RCC reset reason flags before clearing
  watchdogCheckResetReason();

  timeInit();
  I2C1Init();
  lcd_init();
  buttonInit();
  buzzerInit();
  ADCInit();
  calInit(); // Load calibration & protection settings from Flash
  protectionInit(); // Initialize protection relay / optocoupler pin (PB2)
  updateRunningText(); // Pre-populate running text buffer for standby mode

  // Configure PC13 (Status LED on BlackPill) as output
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->MODER &= ~(0x3 << (13 * 2));
  GPIOC->MODER |= (0x1 << (13 * 2));

  // If reset was caused by the watchdog (e.g. RF EMI lockup / crash recovery), notify user
  if (watchdogWasResetByIWDG()) {
    lcdCursor(0, 0);
    lcdString("*WATCHDOG RESET*");
    lcdCursor(1, 0);
    lcdString("IWDG RF Recovery");
    buzzerBeep(150); // Audible notification of watchdog reboot

    // Wait up to 2.5s or until any button is pressed
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

  // Initialize hardware watchdog for RF EMI and crash recovery
  watchdogInit();

  while (1) {
    watchdogRefresh();
    buzzerUpdate();
    readReading();
    protectionCheck(calibratedFWD, calibratedRAD, calculatedSWRValue);

    if (updateReadings()) {
      displayMenu(currentMenu);
    }
  }
}
