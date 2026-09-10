#include "stm32f401xc.h"
#include <stdint.h>

volatile uint32_t millis = 0;

void timeInit(void) {
  SysTick->CTRL |= 0x7;
  SysTick->LOAD = 15999;
  SysTick->VAL = 0;
}

void SysTick_Handler(void) { millis++; }

int main(void) {
  timeInit();
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  GPIOC->MODER &= ~(0x3 << (13 * 2));
  GPIOC->MODER |= (0x1 << (13 * 2));

  uint32_t timeDelay = 0;
  while (1) {
    if (millis - timeDelay >= 1000) {
      timeDelay = millis;
      GPIOC->ODR ^= (0x1 << 13);
    }
  }
}
