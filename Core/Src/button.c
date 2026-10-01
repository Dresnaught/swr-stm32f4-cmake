#include "button.h"
#include "stm32f401xc.h"
#include "sytick.h"

static bool lastButtonState[3] = {false, false, false};
static uint32_t lastPressTime[3] = {0, 0, 0};
static uint32_t lastRepeatTime[3] = {0, 0, 0};
static uint32_t pressDownTime[3] = {0, 0, 0};
static bool holdFired[3] = {false, false, false};
static bool releaseArmed[3] = {false, false, false};

void buttonInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  GPIOB->MODER &= ~(0xFC << 24); // Clear mode for PB12-PB15
  GPIOB->PUPDR &= ~(0xFC << 24); // Clear pull-up/pull-down for PB12-PB15
  GPIOB->PUPDR |= (0x55 << 24);  // Set pull-up for PB12-PB15
}

bool buttonRead(uint8_t button) {
  if (button < 13 || button > 15) {
    return false; // Invalid button number
  }
  return (GPIOB->IDR & (1 << button)) == 0; // Active low
}

bool buttonJustPressed(uint8_t button) {
  if (button < 13 || button > 15) return false;
  uint8_t idx = button - 13;
  bool isPressed = buttonRead(button);
  bool triggered = false;

  if (isPressed && !lastButtonState[idx]) {
    if ((now - lastPressTime[idx]) > 30) { // 30ms debounce
      triggered = true;
      lastPressTime[idx] = now;
      lastRepeatTime[idx] = now + 400; // 400ms delay before repeat kicks in
    }
  }

  lastButtonState[idx] = isPressed;
  return triggered;
}

bool buttonRepeat(uint8_t button) {
  if (button < 13 || button > 15) return false;
  uint8_t idx = button - 13;
  bool isPressed = buttonRead(button);

  if (!isPressed) {
    lastButtonState[idx] = false;
    return false;
  }

  if (!lastButtonState[idx]) {
    // Initial press
    lastButtonState[idx] = true;
    lastPressTime[idx] = now;
    lastRepeatTime[idx] = now + 400;
    return true;
  }

  // Already held down, check repeat interval
  if (now >= lastRepeatTime[idx]) {
    lastRepeatTime[idx] = now + 120; // 120ms repeat rate
    return true;
  }

  return false;
}

bool buttonLongHold(uint8_t button, uint32_t holdMs) {
  if (button < 13 || button > 15) return false;
  uint8_t idx = button - 13;
  bool isPressed = buttonRead(button);

  if (isPressed) {
    if (pressDownTime[idx] == 0) {
      pressDownTime[idx] = now ? now : 1;
      holdFired[idx] = false;
      releaseArmed[idx] = true;
    }
    if (!holdFired[idx] && (now - pressDownTime[idx]) >= holdMs) {
      holdFired[idx] = true;
      releaseArmed[idx] = false;
      return true;
    }
  } else {
    if (pressDownTime[idx] != 0 && (holdFired[idx] || (now - pressDownTime[idx] > 2000))) {
      pressDownTime[idx] = 0;
      holdFired[idx] = false;
      releaseArmed[idx] = false;
    }
  }
  return false;
}

bool buttonShortRelease(uint8_t button) {
  if (button < 13 || button > 15) return false;
  uint8_t idx = button - 13;
  bool isPressed = buttonRead(button);

  if (isPressed) {
    if (pressDownTime[idx] == 0) {
      pressDownTime[idx] = now ? now : 1;
      holdFired[idx] = false;
      releaseArmed[idx] = true;
    }
    return false;
  }

  if (pressDownTime[idx] != 0) {
    uint32_t duration = now - pressDownTime[idx];
    bool valid = releaseArmed[idx] && (!holdFired[idx]) && (duration >= 30);
    pressDownTime[idx] = 0;
    holdFired[idx] = false;
    releaseArmed[idx] = false;
    return valid;
  }

  return false;
}