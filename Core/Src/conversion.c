#include "conversion.h"
#include "calibration.h"
#include "adc.h"
#include <stdint.h>

uint16_t calibratedFWD = 0;
uint16_t calibratedREF = 0;
uint16_t calibratedRAD = 0;
uint16_t calculatedSWRValue = 100;
float calculatedSWRFloatValue = 1.00f;

static uint32_t isqrt(uint32_t val) {
  uint32_t temp, g = 0;
  uint32_t b = 0x8000;

  if (val == 0)
    return 0;

  while (b > 0) {
    temp = g + b;
    if (temp * temp <= val) {
      g = temp;
    }
    b >>= 1;
  }
  return g;
}

uint16_t calculateSWR(uint16_t fwdPower, uint16_t refPower) {
  if (fwdPower == 0)
    return 999;
  if (refPower >= fwdPower)
    return 999;
  if (refPower == 0)
    return 100;

  uint32_t sqrtFWD = isqrt((uint32_t)fwdPower);
  uint32_t sqrtREF = isqrt((uint32_t)refPower);

  uint32_t numerator = (sqrtFWD + sqrtREF) * 100;
  uint32_t denominator = sqrtFWD - sqrtREF;

  if (denominator == 0)
    return 999;

  return (uint16_t)(numerator / denominator);
}

void readReading(void) {
  uint16_t rawadcFWD = 0;
  uint16_t rawadcREF = 0;
  uint16_t rawadcRAD = 0;

  adcCH0Raw(&rawadcFWD);
  adcCH1Raw(&rawadcREF);
  adcCH2Raw(&rawadcRAD);

  calibratedFWD = calInterpolate(CAL_CH_FWD, rawadcFWD);
  calibratedREF = calInterpolate(CAL_CH_REF, rawadcREF);
  calibratedRAD = calInterpolate(CAL_CH_RAD, rawadcRAD);
  calculatedSWRValue = calculateSWR(calibratedFWD, calibratedREF);
  calculatedSWRFloatValue = (float)calculatedSWRValue / 100.0f;
}
