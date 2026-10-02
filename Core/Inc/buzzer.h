#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize buzzer GPIO pin
 * Default for STM32 prototyping: PB0
 * Target STC8 production board: P2.6
 */
void buzzerInit(void);

/**
 * @brief Set buzzer output state directly
 * @param active true = buzzer sounding, false = silent
 */
void buzzerSet(bool active);

/**
 * @brief Get current buzzer output state
 */
bool buzzerGet(void);

/**
 * @brief Trigger a non-blocking beep of specified duration
 * @param durationMs Duration in milliseconds
 */
void buzzerBeep(uint16_t durationMs);

/**
 * @brief Update buzzer state (call periodically in main loop)
 */
void buzzerUpdate(void);

#endif // BUZZER_H
