#include "stm32f401xc.h"
#include "button.h"

void buttonInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  GPIOB->MODER &= ~(0xFC << 24); // Clear mode for PB12-PB15
  GPIOB->PUPDR &= ~(0xFC << 24); // Clear pull-up/pull-down for PB12-PB15
  GPIOB->PUPDR |= (0x55 << 24);  // Set pull-up for PB12-PB15
}

bool buttonRead(uint8_t button) {
  if (button < 13 || button > 15) {
    return false; // Invalid button number
  }
  return (GPIOB->IDR & (1 << button)) == 0; // Return true if button is pressed (active low)
}