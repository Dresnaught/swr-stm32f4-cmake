#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

// Hardware reset reason detected from RCC->CSR
typedef enum {
  RESET_REASON_UNKNOWN = 0,
  RESET_REASON_POR,       // Power-on / brownout reset
  RESET_REASON_PIN,       // NRST pin reset
  RESET_REASON_SOFTWARE,  // Software reset
  RESET_REASON_IWDG,      // Watchdog timeout (crash recovery)
  RESET_REASON_WWDG,      // Window watchdog reset
  RESET_REASON_LPWR       // Low power reset
} ResetReason_t;

// Subsystem breadcrumb checkpoints to trace watchdog timeouts
typedef enum {
  WDG_LOC_UNKNOWN = 0,
  WDG_LOC_MAIN_LOOP,     // Normal main measurement loop
  WDG_LOC_ADC_READ,      // ADC sampling / conversion
  WDG_LOC_LCD_I2C,       // I2C bus transmission to LCD
  WDG_LOC_CAL_SAMPLE,    // In-system calibration sampling
  WDG_LOC_FLASH_WRITE,   // Flash Sector 5 erase/program
  WDG_LOC_USER_TEST      // Intentional watchdog test
} WatchdogLocation_t;

// Check reset flags early in startup and read crash breadcrumb
void watchdogCheckResetReason(void);

// Reset reason helpers
ResetReason_t watchdogGetResetReason(void);
const char* watchdogGetResetReasonStr(void);
bool watchdogWasResetByIWDG(void);

// Crash breadcrumb tracking
void watchdogSetLocation(WatchdogLocation_t loc);
WatchdogLocation_t watchdogGetLastLocation(void);
const char* watchdogGetCrashReasonStr(void);

// Watchdog control
void watchdogInit(void);
void watchdogRefresh(void);
void watchdogTriggerResetTest(void);

#endif // WATCHDOG_H
