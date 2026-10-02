#include "buzzer.h"
#include "stm32f401xc.h"
#include "sytick.h"

// STM32F4 BlackPill prototyping pin: PB0
#define BUZZER_PIN 0

static volatile uint32_t buzzerEndTime = 0;
static bool buzzerState = false;

void buzzerInit(void) {
  // Enable GPIOB clock
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

  // Set PB0 to General Purpose Output mode (01)
  GPIOB->MODER &= ~(0x3U << (BUZZER_PIN * 2));
  GPIOB->MODER |=  (0x1U << (BUZZER_PIN * 2));

  // Output type: Push-Pull (0)
  GPIOB->OTYPER &= ~(1U << BUZZER_PIN);

  // Speed: Low (00)
  GPIOB->OSPEEDR &= ~(0x3U << (BUZZER_PIN * 2));

  // Pull-up/pull-down: None (00)
  GPIOB->PUPDR &= ~(0x3U << (BUZZER_PIN * 2));

  // Start off (low)
  buzzerSet(false);
}

void buzzerSet(bool active) {
  buzzerState = active;
  if (active) {
    GPIOB->BSRR = (1U << BUZZER_PIN); // Set pin High
  } else {
    GPIOB->BSRR = (1U << (BUZZER_PIN + 16)); // Set pin Low (Reset)
  }
}

bool buzzerGet(void) {
  return buzzerState;
}

void buzzerBeep(uint16_t durationMs) {
  if (durationMs == 0) {
    buzzerSet(false);
    buzzerEndTime = 0;
    return;
  }
  buzzerSet(true);
  buzzerEndTime = now + durationMs;
}

void buzzerUpdate(void) {
  if (buzzerEndTime > 0) {
    if (now >= buzzerEndTime) {
      buzzerSet(false);
      buzzerEndTime = 0;
    }
  }
}
