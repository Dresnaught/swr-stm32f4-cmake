#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Non-volatile storage hardware abstraction interface.
 * 
 * Separates core platform-specific flash/EEPROM operations (STM32, STC8, etc.)
 * from portable calibration and application business logic.
 */

// Initialize storage driver
bool storageInit(void);

// Read raw buffer from non-volatile storage
bool storageRead(uint32_t offset, void *buffer, size_t size);

// Write raw buffer to non-volatile storage (handles unlock, programming, and re-lock)
bool storageWrite(uint32_t offset, const void *data, size_t size);

// Erase calibration storage sector/block
bool storageErase(void);

#endif // STORAGE_H
