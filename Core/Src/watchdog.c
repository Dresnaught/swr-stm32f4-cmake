#include "watchdog.h"
#include "stm32f401xc.h"

#define WDG_MAGIC 0x57444731U // 'WDG1'

typedef struct {
  uint32_t magic;
  uint32_t location;
} WatchdogBreadcrumb_t;

// Persistent memory across watchdog / warm resets (placed in .noinit section)
__attribute__((section(".noinit")))
static WatchdogBreadcrumb_t crashBreadcrumb;

static ResetReason_t detectedResetReason = RESET_REASON_UNKNOWN;
static WatchdogLocation_t bootCrashLocation = WDG_LOC_UNKNOWN;

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

  // Clear hardware reset flags
  RCC->CSR |= RCC_CSR_RMVF;

  // If rebooted by watchdog, read crash breadcrumb from persistent RAM
  if (detectedResetReason == RESET_REASON_IWDG && crashBreadcrumb.magic == WDG_MAGIC) {
    bootCrashLocation = (WatchdogLocation_t)crashBreadcrumb.location;
  } else {
    bootCrashLocation = WDG_LOC_UNKNOWN;
  }

  // Initialize breadcrumb for this run
  crashBreadcrumb.magic = WDG_MAGIC;
  crashBreadcrumb.location = WDG_LOC_MAIN_LOOP;
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
    default:                    return "Normal Boot";
  }
}

bool watchdogWasResetByIWDG(void) {
  return (detectedResetReason == RESET_REASON_IWDG);
}

void watchdogSetLocation(WatchdogLocation_t loc) {
  crashBreadcrumb.magic = WDG_MAGIC;
  crashBreadcrumb.location = (uint32_t)loc;
}

WatchdogLocation_t watchdogGetLastLocation(void) {
  return bootCrashLocation;
}

const char* watchdogGetCrashReasonStr(void) {
  if (detectedResetReason != RESET_REASON_IWDG) {
    return watchdogGetResetReasonStr();
  }

  switch (bootCrashLocation) {
    case WDG_LOC_USER_TEST:   return "Manual Test";
    case WDG_LOC_LCD_I2C:     return "I2C Bus Lock";
    case WDG_LOC_ADC_READ:    return "ADC Read Hang";
    case WDG_LOC_FLASH_WRITE: return "Flash Write Hang";
    case WDG_LOC_CAL_SAMPLE:  return "Cal Sample Hang";
    case WDG_LOC_MAIN_LOOP:   return "Main Loop Stall";
    default:                  return "RF EMI Lockup";
  }
}

void watchdogInit(void) {
  // Unlock watchdog registers
  IWDG->KR = 0x5555;

  // Prescaler: 256 (with 32 kHz LSI, ~125 Hz clock, 8 ms per tick)
  IWDG->PR = 6;

  // Reload: 1000 ticks = ~8.0 seconds timeout
  IWDG->RLR = 1000;

  // Load counter
  IWDG->KR = 0xAAAA;

  // Start counter
  IWDG->KR = 0xCCCC;
}

void watchdogRefresh(void) {
  IWDG->KR = 0xAAAA;
}

void watchdogTriggerResetTest(void) {
  watchdogSetLocation(WDG_LOC_USER_TEST);
  // Freeze CPU to verify watchdog reset
  while (1) {
    __NOP();
  }
}
