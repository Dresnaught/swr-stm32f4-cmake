#include "lcd.h"
#include "adc.h"
#include "conversion.h"
#include "calibration.h"
#include "protection.h"
#include "button.h"
#include "menu.h"
#include "stm32f401xc.h"
#include "sytick.h"
#include <stdint.h>
#include <stdbool.h>

int main(void) {
  timeInit();
  I2C1Init();
  lcd_init();
  buttonInit();
  ADCInit();
  calInit(); // Load calibration & protection settings from Flash
  protectionInit(); // Initialize protection relay / optocoupler pin (PB2)

  // Configure PC13 (Status LED on BlackPill) as output
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->MODER &= ~(0x3 << (13 * 2));
  GPIOC->MODER |= (0x1 << (13 * 2));

  lcdCursor(0, 0);
  lcdString("SWR Power Meter");
  lcdCursor(1, 0);
  lcdString("Digital STC8/F4");
  delay_ms(900);

  while (1) {
    readReading();
    protectionCheck(calibratedFWD, calibratedRAD, calculatedSWRValue);

    if (updateReadings()) {
      displayMenu(currentMenu);
    }
  }
}
