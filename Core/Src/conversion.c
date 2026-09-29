#include "conversion.h"
#include "adc.h"
#include <stdint.h>

const CalPoint_t fwdCalTable[7] = {
    {0, 0},      // table 0
    {500, 10},   // table 1
    {1000, 50},  // table 2
    {1500, 100}, // table 3
    {2000, 250}, // table 4
    {3000, 500}, // table 5
    {4095, 1000} // table 6
};

const CalPoint_t refCalTable[7] = {
    {0, 0},      // table 0
    {400, 5},    // table 1
    {900, 20},   // table 2
    {1300, 50},  // table 3
    {1800, 100}, // table 4
    {2800, 200}, // table 5
    {4095, 500}  // table 6
};

const CalPoint_t radCalTable[7] = {
    {0, 0},      // table 0
    {600, 5},    // table 1
    {1100, 15},  // table 2
    {1600, 30},  // table 3
    {2200, 50},  // table 4
    {3200, 80},  // table 5
    {4095, 1500} // table 6
};

// Core Piecewise Linear Interpolation Function
uint16_t interpolate(uint16_t rawInput, const CalPoint_t *table, uint8_t size) {
  // 1. Handle out-of-bounds (below minimum)
  if (rawInput <= table[0].raw) {
    return table[0].value;
  }
  // 2. Handle out-of-bounds (above maximum)
  if (rawInput >= table[size - 1].raw) {
    return table[size - 1].value;
  }

  // 3. Find the segment where the rawInput fits
  for (uint8_t i = 0; i < size - 1; i++) {
    if (rawInput >= table[i].raw && rawInput <= table[i + 1].raw) {
      // Linear interpolation formula: y = y0 + ((x - x0) * (y1 - y0)) / (x1 -
      // x0)
      uint32_t x0 = table[i].raw;
      uint32_t x1 = table[i + 1].raw;
      uint32_t y0 = table[i].value;
      uint32_t y1 = table[i + 1].value;

      return (uint16_t)(y0 + ((rawInput - x0) * (y1 - y0)) / (x1 - x0));
    }
  }

  return 0; // Fallback
}

static uint32_t isqrt(uint32_t val) {
  uint32_t temp, g = 0;
  uint32_t b = 0x8000; // Start at the highest bit for 16-bit results

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

/**
 * Calculates SWR multiplied by 100 (e.g., 1.50 SWR returns 150)
 * Expects fwdPower and refPower in the same units (e.g., Milliwatts or Watts)
 */
uint16_t calculateSWR(uint16_t fwdPower, uint16_t refPower) {
  // 1. Handle edge cases to prevent division by zero or negative SWR
  if (fwdPower == 0)
    return 999; // No forward power -> invalid SWR (9.99)
  if (refPower >= fwdPower)
    return 999; // Total reflection -> Max SWR (9.99)
  if (refPower == 0)
    return 100; // Perfect match -> 1.00 SWR

  // 2. Take integer square roots of power to get relative voltages
  uint32_t sqrtFWD = isqrt((uint32_t)fwdPower);
  uint32_t sqrtREF = isqrt((uint32_t)refPower);

  // 3. SWR = 100 * (sqrtFWD + sqrtREF) / (sqrtFWD - sqrtREF)
  uint32_t numerator = (sqrtFWD + sqrtREF) * 100;
  uint32_t denominator = sqrtFWD - sqrtREF;

  if (denominator == 0)
    return 999;

  return (uint16_t)(numerator / denominator);
}

uint16_t calibratedFWD;
uint16_t calibratedREF;
uint16_t calibratedRAD;
uint16_t calculatedSWRValue;
float calculatedSWRFloatValue;

// Reads ADC values, calibrates them, and calculates SWR
void readReading(void) {
  uint16_t rawadcFWD = 0;
  uint16_t rawadcREF = 0;
  uint16_t rawadcRAD = 0;

  adcCH0Raw(&rawadcFWD);
  adcCH1Raw(&rawadcREF);
  adcCH2Raw(&rawadcRAD);

  calibratedFWD = interpolate(rawadcFWD, fwdCalTable, 7);
  calibratedREF = interpolate(rawadcREF, refCalTable, 7);
  calibratedRAD = interpolate(rawadcRAD, radCalTable, 7);
  calculatedSWRValue = calculateSWR(calibratedFWD, calibratedREF);
  calculatedSWRFloatValue = (float)calculatedSWRValue / 100.0f;
}
