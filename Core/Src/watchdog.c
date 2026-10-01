#include "watchdog.h"
#include "stm32f401xc.h"

void watchdogInit(void) {
  // 1. Enable register access by writing 0x5555 to KR
  IWDG->KR = 0x5555;

  // 2. Set prescaler: PR = 6 (Divider = 256)
  // With LSI ~32 kHz, clock rate is ~125 Hz (8 ms per tick)
  IWDG->PR = 6;

  // 3. Set reload value: RLR = 1000 -> 1000 * 8 ms = 8000 ms (8.0s timeout)
  IWDG->RLR = 1000;

  // 4. Reload counter with value in RLR
  IWDG->KR = 0xAAAA;

  // 5. Start the watchdog counter
  IWDG->KR = 0xCCCC;
}

void watchdogRefresh(void) {
  // Reload counter
  IWDG->KR = 0xAAAA;
}
