#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>

/**
 * @brief Initialize the Independent Watchdog (IWDG)
 * Configures ~2.0 second timeout using the 32 kHz LSI clock.
 */
void watchdogInit(void);

/**
 * @brief Refresh / kick the Independent Watchdog (IWDG)
 * Prevents system reset when called periodically.
 */
void watchdogRefresh(void);

#endif // WATCHDOG_H
