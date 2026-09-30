#ifndef CONVERSION_H
#define CONVERSION_H

#include <stdint.h>
#include "calibration.h"

extern uint16_t calibratedFWD;
extern uint16_t calibratedREF;
extern uint16_t calibratedRAD;
extern uint16_t calculatedSWRValue;
extern float calculatedSWRFloatValue;

uint16_t calculateSWR(uint16_t fwdPower, uint16_t refPower);
void readReading(void);

#endif // CONVERSION_H
