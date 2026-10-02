#include "watchdog.h"
#include "stm32f401xc.h"

static ResetReason_t detectedResetReason = RESET_REASON_UNKNOWN;

void watchdogCheckResetReason(void) {
  uint32_t csr = RCC->CSR;

  if (csr & RCC_CSR_IWDGRSTF) {
    detectedResetReason = RESET_REASON_IWDG;
  } else if (csr & RCC_CSR_WWDGRSTF) {
    detectedResetReason = RESET_REASON_WWDG;
  } else if (csr & RCC_CSR_SFTRSTF) {
    detectedResetReason = RESET_REASON_SOFTWARE;
  } else if (csr & RCC_CSR_PORRSTF) {
    detectedResetReason = RESET_REASON_POR;
  } else if (csr & RCC_CSR_PINRSTF) {
    detectedResetReason = RESET_REASON_PIN;
  } else if (csr & RCC_CSR_LPWRRSTF) {
    detectedResetReason = RESET_REASON_LPWR;
  } else {
    detectedResetReason = RESET_REASON_UNKNOWN;
  }

  // Clear all reset flags
  RCC->CSR |= RCC_CSR_RMVF;
}

ResetReason_t watchdogGetResetReason(void) {
  return detectedResetReason;
}

const char* watchdogGetResetReasonStr(void) {
  switch (detectedResetReason) {
    case RESET_REASON_IWDG:     return "IWDG Watchdog";
    case RESET_REASON_WWDG:     return "WWDG Watchdog";
    case RESET_REASON_SOFTWARE: return "Software Reset";
    case RESET_REASON_POR:      return "Power-On / POR";
    case RESET_REASON_PIN:      return "NRST Pin Reset";
    case RESET_REASON_LPWR:     return "Low Power Reset";
    default:                    return "Normal / Unknown";
  }
}

bool watchdogWasResetByIWDG(void) {
  return (detectedResetReason == RESET_REASON_IWDG);
}

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

void watchdogTriggerResetTest(void) {
  // Stop kicking the watchdog and freeze execution to verify IWDG reboot
  while (1) {
    __NOP();
  }
}
