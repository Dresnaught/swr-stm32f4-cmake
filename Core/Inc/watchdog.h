#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  RESET_REASON_UNKNOWN = 0,
  RESET_REASON_POR,       // Power-On / Brownout Reset
  RESET_REASON_PIN,       // NRST Pin Reset
  RESET_REASON_SOFTWARE,  // Software Reset
  RESET_REASON_IWDG,      // Independent Watchdog Reset (RF Lockup / Crash)
  RESET_REASON_WWDG,      // Window Watchdog Reset
  RESET_REASON_LPWR       // Low Power Reset
} ResetReason_t;

/**
 * @brief Inspect RCC reset flags at boot and clear them
 * Call this early in main() before initializing other peripherals.
 */
void watchdogCheckResetReason(void);

/**
 * @brief Get the last detected reset reason enum
 */
ResetReason_t watchdogGetResetReason(void);

/**
 * @brief Get a human-readable string of the last reset reason
 */
const char* watchdogGetResetReasonStr(void);

/**
 * @brief Returns true if the MCU was rebooted by the hardware watchdog
 */
bool watchdogWasResetByIWDG(void);

/**
 * @brief Initialize the Independent Watchdog (IWDG)
 * Configures ~8.0 second timeout using the 32 kHz LSI clock.
 */
void watchdogInit(void);

/**
 * @brief Refresh / kick the Independent Watchdog (IWDG)
 * Prevents system reset when called periodically.
 */
void watchdogRefresh(void);

/**
 * @brief Intentional halt to test watchdog reset behavior
 */
void watchdogTriggerResetTest(void);

#endif // WATCHDOG_H
