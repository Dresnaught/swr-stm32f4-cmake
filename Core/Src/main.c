//#include "lcd.h"
#include "stm32f401xc.h"
#include "sytick.h"
#include <stdint.h>



int main(void) {
  timeInit();
  //  I2C1Init();
  //  lcd_init();
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->MODER &= ~(0x3 << (13 * 2));
  GPIOC->MODER |= (0x1 << (13 * 2));
  /*lcdCursor(0, 0);
  lcdString("Hello World");
  lcdCursor(1, 0);
  lcdFloat(2.250);*/

  uint32_t timeDelay = 0;
  while (1) {
     if (millis - timeDelay >= 1000) {
     timeDelay = millis;
     GPIOC->ODR ^= (0x1 << 13);
    }
  }
}
