#include "protection.h"
#include "stm32f401xc.h"
#include "calibration.h"
#include "buzzer.h"
#include "menu.h"
#include "button.h"

static bool relayState = true;
static bool isTripped = false;
static TripCause_t currentTripCause = TRIP_NONE;

void protectionInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Enable GPIOB clock
  GPIOB->MODER &= ~(0x3 << (2 * 2));   // Clear mode bits for PB2
  GPIOB->MODER |= (0x1 << (2 * 2));    // Set PB2 as output
  GPIOB->OTYPER &= ~(0x1 << 2);        // Push-pull
  GPIOB->OSPEEDR |= (0x3 << (2 * 2));  // High speed
  GPIOB->PUPDR &= ~(0x3 << (2 * 2));   // No pull-up/pull-down

  protectionLogicInit();
}

void protectionSet(bool state) {
  relayState = state;
  if (state) {
    GPIOB->BSRR = (1 << 2);            // Set PB2 high (Optocoupler ON / Normal)
  } else {
    GPIOB->BSRR = (1 << (2 + 16));     // Set PB2 low (Optocoupler OFF / Cutoff)
  }
}

void protectionLogicInit(void) {
  isTripped = false;
  currentTripCause = TRIP_NONE;
  protectionSet(true); // Normal state: optocoupler enabled
  buzzerSet(false);
}

void protectionTrip(TripCause_t cause) {
  isTripped = true;
  currentTripCause = cause;
  protectionSet(false); // Instantly de-energize optocoupler / relay
  buzzerSet(true);      // Sound buzzer on fault
  currentMenu = MAIN_SCREEN; // Auto-protect: immediately switch to trip alert screen
  buttonClearAll();
}

void protectionReset(void) {
  isTripped = false;
  currentTripCause = TRIP_NONE;
  protectionSet(true);
  buzzerSet(false);     // Silence buzzer
}

bool protectionIsTripped(void) {
  return isTripped;
}

TripCause_t protectionGetTripCause(void) {
  return currentTripCause;
}

const char* protectionGetTripCauseString(void) {
  switch (currentTripCause) {
    case TRIP_RAD_OVERPOWER: return "RAD OVERPOWER";
    case TRIP_HIGH_SWR:      return "HIGH SWR";
    default:                 return "OK";
  }
}

bool protectionIsEnabled(void) {
  return activeCal.protection.enabled != 0;
}

void protectionSetEnabled(bool enable) {
  activeCal.protection.enabled = enable ? 1 : 0;
  if (!enable) {
    protectionReset();
  }
}

void protectionToggleEnabled(void) {
  protectionSetEnabled(!protectionIsEnabled());
}

void protectionCheck(uint16_t fwdWatts, uint16_t radWatts, uint16_t swrX100) {
  if (!protectionIsEnabled()) {
    protectionSet(true);
    return;
  }
  if (isTripped) {
    // Latched off until reset
    protectionSet(false);
    return;
  }

  if (activeCal.protection.enabled == 0) {
    protectionSet(true);
    return;
  }

  // Trigger 1: RAD Input Over-Power Protection (1 to 50W)
  uint16_t radLimit = activeCal.protection.radLimitWatts;
  if (radLimit > 0 && radWatts > radLimit) {
    protectionTrip(TRIP_RAD_OVERPOWER);
    return;
  }

  // Trigger 2: High SWR Protection (1.1 to 5.0)
  // Only evaluate SWR if forward power is actively transmitting (e.g. >= 2W or rad >= 1W)
  uint16_t swrLimit = activeCal.protection.swrLimitX100;
  if (swrLimit > 0 && (fwdWatts >= 2 || radWatts >= 1)) {
    if (swrX100 > swrLimit) {
      protectionTrip(TRIP_HIGH_SWR);
      return;
    }
  }

  // Normal safe operation
  protectionSet(true);
}

uint16_t protectionGetRadLimit(void) {
  return activeCal.protection.radLimitWatts;
}

void protectionSetRadLimit(uint16_t watts) {
  if (watts > 50) watts = 50;
  activeCal.protection.radLimitWatts = watts;
}

// Stepping: OFF(0) -> 1-2-3...10 by 1, then 10-15-20...50 by 5
uint16_t stepRadLimitUp(uint16_t current) {
  if (current == 0) return 1;
  if (current < 10) return current + 1;
  if (current < 50) return current + 5;
  return 50;
}

uint16_t stepRadLimitDown(uint16_t current) {
  if (current <= 1) return 0; // Turn OFF
  if (current <= 10) return current - 1;
  return current - 5;
}

uint16_t protectionGetSwrLimit(void) {
  return activeCal.protection.swrLimitX100;
}

void protectionSetSwrLimit(uint16_t swrX100) {
  if (swrX100 > 500) swrX100 = 500;
  activeCal.protection.swrLimitX100 = swrX100;
}

// Stepping: OFF(0) -> 1.1 to 5.0 by 0.1 (110 to 500 by 10)
uint16_t stepSwrLimitUp(uint16_t current) {
  if (current == 0) return 110;
  if (current < 110) return 110;
  if (current + 10 <= 500) return current + 10;
  return 500;
}

uint16_t stepSwrLimitDown(uint16_t current) {
  if (current <= 110) return 0; // Turn OFF
  return current - 10;
}

bool protectionGetRelayState(void) {
  return relayState;
}

void protectionToggleRelay(void) {
  protectionSet(!relayState);
}