#include "storage.h"
#include "stm32f401xc.h"
#include <string.h>

#define FLASH_STORAGE_BASE  0x08020000U // Sector 5 start address on STM32F401xC
#define FLASH_STORAGE_SECTOR 5U         // Sector 5
#define FLASH_KEY1          0x45670123U
#define FLASH_KEY2          0xCDEF89ABU

static void flashUnlock(void) {
  if (FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }
}

static void flashLock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

static void flashWaitBusy(void) {
  while (FLASH->SR & FLASH_SR_BSY) {
    __NOP();
  }
}

bool storageInit(void) {
  return true;
}

bool storageRead(uint32_t offset, void *buffer, size_t size) {
  if (!buffer) return false;
  const uint8_t *src = (const uint8_t *)(FLASH_STORAGE_BASE + offset);
  memcpy(buffer, src, size);
  return true;
}

bool storageErase(void) {
  flashUnlock();
  flashWaitBusy();

  // Clear previous error status flags
  FLASH->SR = FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR | 
              FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

  // Sector Erase: Sector 5
  FLASH->CR &= ~FLASH_CR_SNB;
  FLASH->CR |= (FLASH_STORAGE_SECTOR << FLASH_CR_SNB_Pos) | FLASH_CR_SER;
  FLASH->CR |= FLASH_CR_STRT;

  flashWaitBusy();
  FLASH->CR &= ~FLASH_CR_SER;
  flashLock();

  return true;
}

bool storageWrite(uint32_t offset, const void *data, size_t size) {
  if (!data || size == 0) return false;

  // First erase the sector before reprogramming
  if (!storageErase()) return false;

  flashUnlock();
  flashWaitBusy();

  // Clear status errors
  FLASH->SR = FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR | 
              FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

  // Configure parallelism for 32-bit programming (PSIZE = 10b)
  FLASH->CR &= ~FLASH_CR_PSIZE;
  FLASH->CR |= FLASH_CR_PSIZE_1;

  uint32_t destAddr = FLASH_STORAGE_BASE + offset;
  const uint32_t *src = (const uint32_t *)data;
  size_t words = (size + 3) / 4; // Round up to full 32-bit words

  for (size_t i = 0; i < words; i++) {
    FLASH->CR |= FLASH_CR_PG;
    *(__IO uint32_t *)(destAddr + (i * 4)) = src[i];
    flashWaitBusy();
    FLASH->CR &= ~FLASH_CR_PG;

    // Check for programming errors
    if (FLASH->SR & (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)) {
      flashLock();
      return false;
    }
  }

  flashLock();
  return true;
}
