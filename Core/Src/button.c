#include "button.h"
#include "stm32f401xc.h"
#include "sytick.h"

// Stable debounced button states (true = pressed / low)
static bool debouncedState[3]      = {false, false, false};
static bool lastRawState[3]        = {false, false, false};
static uint32_t lastChangeTime[3]  = {0, 0, 0};

// Event flags consumed on query
static bool justPressedFlag[3]     = {false, false, false};
static bool shortReleaseFlag[3]    = {false, false, false};
static uint32_t pressStartTime[3]  = {0, 0, 0};
static uint32_t nextRepeatTime[3]  = {0, 0, 0};
static bool holdFiredFlag[3]       = {false, false, false};
static bool releaseArmedFlag[3]    = {false, false, false};

void buttonInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  // PB13 (UP), PB14 (SELECT), PB15 (DOWN) as Input with Pull-Up
  GPIOB->MODER &= ~(0x3FU << (13 * 2)); // Inputs (00b)
  GPIOB->PUPDR &= ~(0x3FU << (13 * 2));
  GPIOB->PUPDR |=  (0x15U << (13 * 2)); // Pull-Up (01b)

  for (uint8_t i = 0; i < 3; i++) {
    debouncedState[i] = false;
    lastRawState[i] = false;
    lastChangeTime[i] = 0;
    justPressedFlag[i] = false;
    shortReleaseFlag[i] = false;
    pressStartTime[i] = 0;
    nextRepeatTime[i] = 0;
    holdFiredFlag[i] = false;
    releaseArmedFlag[i] = false;
  }
}

bool buttonRead(uint8_t button) {
  if (button < 13 || button > 15) return false;
  return (GPIOB->IDR & (1U << button)) == 0; // Active low
}

bool buttonAnyPressed(void) {
  uint32_t idr = GPIOB->IDR;
  // Check if any of PB13, PB14, PB15 are pulled low (pressed)
  return ((~idr) & ((1U << 13) | (1U << 14) | (1U << 15))) != 0;
}

void buttonUpdate(void) {
  uint32_t idr = GPIOB->IDR;
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t pin = 13 + i;
    bool raw = ((idr & (1U << pin)) == 0); // Active low: true when pressed

    if (raw != lastRawState[i]) {
      lastRawState[i] = raw;
      lastChangeTime[i] = now;
    }

    // Require 25ms of stable reading before committing state transitions
    if ((now - lastChangeTime[i]) >= 25) {
      if (debouncedState[i] != raw) {
        debouncedState[i] = raw;
        if (raw) {
          // Transition: Released -> Pressed
          justPressedFlag[i] = true;
          pressStartTime[i] = now ? now : 1;
          nextRepeatTime[i] = now + 350;
          holdFiredFlag[i] = false;
          releaseArmedFlag[i] = true;
        } else {
          // Transition: Pressed -> Released
          if (releaseArmedFlag[i] && !holdFiredFlag[i]) {
            shortReleaseFlag[i] = true;
          }
          releaseArmedFlag[i] = false;
          holdFiredFlag[i] = false;
          pressStartTime[i] = 0;
        }
      }
    }
  }
}

void buttonClearAll(void) {
  buttonUpdate();
  for (uint8_t i = 0; i < 3; i++) {
    justPressedFlag[i] = false;
    shortReleaseFlag[i] = false;
    releaseArmedFlag[i] = false;
    holdFiredFlag[i] = false;
    nextRepeatTime[i] = 0;
  }
}

bool buttonJustPressed(uint8_t button) {
  if (button < 13 || button > 15) return false;
  buttonUpdate();
  uint8_t idx = button - 13;
  if (justPressedFlag[idx]) {
    justPressedFlag[idx] = false; // Consumed on read
    return true;
  }
  return false;
}

bool buttonShortRelease(uint8_t button) {
  if (button < 13 || button > 15) return false;
  buttonUpdate();
  uint8_t idx = button - 13;
  if (shortReleaseFlag[idx]) {
    shortReleaseFlag[idx] = false; // Consumed on read
    return true;
  }
  return false;
}

bool buttonLongHold(uint8_t button, uint32_t holdMs) {
  if (button < 13 || button > 15) return false;
  buttonUpdate();
  uint8_t idx = button - 13;
  if (debouncedState[idx] && !holdFiredFlag[idx] && pressStartTime[idx] > 0) {
    if ((now - pressStartTime[idx]) >= holdMs) {
      holdFiredFlag[idx] = true;
      releaseArmedFlag[idx] = false; // Disarm release to prevent trailing click
      return true;
    }
  }
  return false;
}

bool buttonRepeat(uint8_t button) {
  if (button < 13 || button > 15) return false;
  buttonUpdate();
  uint8_t idx = button - 13;
  // First press down fires immediately
  if (justPressedFlag[idx]) {
    justPressedFlag[idx] = false;
    return true;
  }
  // While held down, repeat at 80ms interval after initial 350ms hold
  if (debouncedState[idx] && now >= nextRepeatTime[idx]) {
    nextRepeatTime[idx] = now + 80;
    return true;
  }
  return false;
}