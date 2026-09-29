#ifndef CONVERSION_H
#define CONVERSION_H
#include <stdint.h>

typedef struct {
  uint16_t raw;
  uint16_t value;
} CalPoint_t;

extern const CalPoint_t fwdCalTable[7];
extern const CalPoint_t refCalTable[7];
extern const CalPoint_t radCalTable[7];

extern uint16_t calibratedFWD;
extern uint16_t calibratedREF;
extern uint16_t calibratedRAD;
extern uint16_t calculatedSWRValue;
extern float calculatedSWRFloatValue;

uint16_t interpolate(uint16_t rawInput, const CalPoint_t *table, uint8_t size);
uint16_t calculateSWR(uint16_t fwdPower, uint16_t refPower);
void readReading(void);

#endif // !CONVERSION_H
