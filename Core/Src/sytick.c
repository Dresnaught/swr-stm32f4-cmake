#include "sytick.h"
#include "stm32f401xc.h"

// (track how many time the MCU ran) by milliseconds
volatile uint32_t now = 0;

void timeInit(void) {
  SysTick->CTRL &= ~0x7;
  SysTick->LOAD = 15999;
  SysTick->VAL = 0;
  SysTick->CTRL |= 0x7;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable Trace Architecture
  DWT->CYCCNT = 0;                                // Reset cycle counter
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable cycle counter
}

__attribute__((used)) void SysTick_Handler(void) {
  now++;
}

void delay_ms(uint32_t ms) {
  uint32_t delay = now;
  while ((now - delay) < ms) {
    __NOP();
  }
}

void delay_us(uint32_t us) {
  uint32_t us_ticks = us * 16;
  uint32_t start_cycles = DWT->CYCCNT;

  while ((DWT->CYCCNT - start_cycles) < us_ticks) {
    __NOP();
  }
}
