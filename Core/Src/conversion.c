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
  uint32_t b = 0x8000; // Bit 15 for up to 32-bit values

  if (val == 0) return 0;

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
  // If not transmitting or forward power is zero, SWR is idle 1.00
  if (fwdPower < 1) {
    return 100;
  }
  // Total reflection (open/short circuit or detector offset)
  if (refPower >= fwdPower) {
    return 999;
  }
  // Perfect match (zero reflected power)
  if (refPower == 0) {
    return 100;
  }

  // Calculate Gamma * 1000 = sqrt((Pref * 1000000) / Pfwd)
  // Scaled by 1,000,000 for 3 decimal places of precision without floats
  uint32_t ratio = ((uint32_t)refPower * 1000000UL) / (uint32_t)fwdPower;
  uint32_t gammaX1000 = isqrt(ratio);

  if (gammaX1000 >= 990) {
    return 999;
  }

  // SWR = (1 + Gamma) / (1 - Gamma)
  // SWR * 100 = ((1000 + GammaX1000) * 100) / (1000 - GammaX1000)
  uint32_t num = (1000UL + gammaX1000) * 100UL;
  uint32_t den = 1000UL - gammaX1000;

  if (den == 0) return 999;

  uint32_t swrX100 = (num + (den / 2)) / den; // Rounded division
  if (swrX100 > 999) swrX100 = 999;
  if (swrX100 < 100) swrX100 = 100;

  return (uint16_t)swrX100;
}

static uint32_t filterFWD = 0;
static uint32_t filterREF = 0;
static uint32_t filterRAD = 0;
static bool filterInit = false;

void readReading(void) {
  uint16_t rawadcFWD = 0;
  uint16_t rawadcREF = 0;
  uint16_t rawadcRAD = 0;

  adcCH0Raw(&rawadcFWD);
  adcCH1Raw(&rawadcREF);
  adcCH2Raw(&rawadcRAD);

  if (!filterInit) {
    filterFWD = (uint32_t)rawadcFWD << 4;
    filterREF = (uint32_t)rawadcREF << 4;
    filterRAD = (uint32_t)rawadcRAD << 4;
    filterInit = true;
  }

  // Smooth fixed-point low-pass filter (EMA):
  // Eliminates 50Hz/60Hz AC mains hum, noise spikes, and erratic digit jumping,
  // while tracking real RF envelope and speech peaks cleanly
  filterFWD = (filterFWD * 3 + ((uint32_t)rawadcFWD << 4)) / 4;
  filterREF = (filterREF * 3 + ((uint32_t)rawadcREF << 4)) / 4;
  filterRAD = (filterRAD * 3 + ((uint32_t)rawadcRAD << 4)) / 4;

  uint16_t smoothFWD = (uint16_t)((filterFWD + 8) >> 4);
  uint16_t smoothREF = (uint16_t)((filterREF + 8) >> 4);
  uint16_t smoothRAD = (uint16_t)((filterRAD + 8) >> 4);

  calibratedFWD = calInterpolate(CAL_CH_FWD, smoothFWD);
  calibratedREF = calInterpolate(CAL_CH_REF, smoothREF);
  calibratedRAD = calInterpolate(CAL_CH_RAD, smoothRAD);

  calculatedSWRValue = calculateSWR(calibratedFWD, calibratedREF);
  calculatedSWRFloatValue = (float)calculatedSWRValue / 100.0f;
}
