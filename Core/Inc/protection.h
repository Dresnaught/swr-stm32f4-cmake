#ifndef PROTECTION_H
#define PROTECTION_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  TRIP_NONE = 0,
  TRIP_RAD_OVERPOWER,
  TRIP_HIGH_SWR
} TripCause_t;

// Hardware driver (Core STM32: PB2; STC8: P2.7)
void protectionInit(void);
void protectionSet(bool state); // true = Pin HIGH (Optocoupler ON), false = Pin LOW (Cutoff / Trip)

// Portable protection engine
void protectionLogicInit(void);
void protectionCheck(uint16_t fwdWatts, uint16_t radWatts, uint16_t swrX100);
void protectionTrip(TripCause_t cause);
void protectionReset(void);
bool protectionIsTripped(void);
TripCause_t protectionGetTripCause(void);
const char* protectionGetTripCauseString(void);

// Thresholds and stepping helpers
uint16_t protectionGetRadLimit(void);
void protectionSetRadLimit(uint16_t watts);
uint16_t stepRadLimitUp(uint16_t current);
uint16_t stepRadLimitDown(uint16_t current);

uint16_t protectionGetSwrLimit(void); // Returns SWR * 100 (e.g. 200 for 2.0 SWR)
void protectionSetSwrLimit(uint16_t swrX100);
uint16_t stepSwrLimitUp(uint16_t current);
uint16_t stepSwrLimitDown(uint16_t current);

bool protectionGetRelayState(void);
void protectionToggleRelay(void);

#endif // PROTECTION_H
