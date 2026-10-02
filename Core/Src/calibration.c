#include "calibration.h"
#include "storage.h"
#include <string.h>
#include <stddef.h>

CalConfig_t activeCal;

static const CalPoint_t defaultFwdPoints[CAL_DEFAULT_POINTS] = {
  {0, 0},
  {500, 10},
  {1000, 50},
  {1500, 100},
  {2000, 250},
  {3000, 500},
  {4095, 1000}
};

static const CalPoint_t defaultRefPoints[CAL_DEFAULT_POINTS] = {
  {0, 0},
  {400, 1},
  {900, 5},
  {1300, 10},
  {1800, 25},
  {2800, 50},
  {4095, 100}
};

static const CalPoint_t defaultRadPoints[CAL_DEFAULT_POINTS] = {
  {0, 0},
  {600, 2},
  {1100, 5},
  {1600, 10},
  {2200, 20},
  {3200, 35},
  {4095, 50}
};

static uint32_t calCalculateChecksum(const CalConfig_t *cfg) {
  const uint8_t *data = (const uint8_t *)cfg;
  size_t len = offsetof(CalConfig_t, checksum);
  uint32_t sum = 0x55AA55AAU;
  for (size_t i = 0; i < len; i++) {
    sum = ((sum << 1) | (sum >> 31)) ^ data[i];
  }
  return sum;
}

void calResetDefaults(void) {
  memset(&activeCal, 0, sizeof(CalConfig_t));
  activeCal.magic = CAL_MAGIC;
  activeCal.version = CAL_VERSION;

  // Forward Power: Max 1000W
  activeCal.channels[CAL_CH_FWD].count = CAL_DEFAULT_POINTS;
  activeCal.channels[CAL_CH_FWD].reserved = 0;
  activeCal.channels[CAL_CH_FWD].maxWatts = 1000;
  memcpy(activeCal.channels[CAL_CH_FWD].points, defaultFwdPoints, sizeof(defaultFwdPoints));

  // Reflected Power: Max 100W (FWD / 10)
  activeCal.channels[CAL_CH_REF].count = CAL_DEFAULT_POINTS;
  activeCal.channels[CAL_CH_REF].reserved = 0;
  activeCal.channels[CAL_CH_REF].maxWatts = 100;
  memcpy(activeCal.channels[CAL_CH_REF].points, defaultRefPoints, sizeof(defaultRefPoints));

  // Radio-In (Drive Power): Max 50W
  activeCal.channels[CAL_CH_RAD].count = CAL_DEFAULT_POINTS;
  activeCal.channels[CAL_CH_RAD].reserved = 0;
  activeCal.channels[CAL_CH_RAD].maxWatts = 50;
  memcpy(activeCal.channels[CAL_CH_RAD].points, defaultRadPoints, sizeof(defaultRadPoints));

  // Protection defaults
  activeCal.protection.radLimitWatts = 15; // 15W default trip for Radio-In
  activeCal.protection.swrLimitX100 = 200; // 2.00 default trip for SWR
  activeCal.protection.enabled = 1;
  activeCal.protection.barStyle = BAR_STYLE_SCALE; // Default to Custom Pixel Scale
  activeCal.protection.runningTextEnabled = 1;     // Default: Running text marquee enabled in standby

  activeCal.checksum = calCalculateChecksum(&activeCal);
}

void calInit(void) {
  storageInit();
  if (!calLoadFromFlash()) {
    // Flash unprogrammed or checksum failed -> initialize defaults
    calResetDefaults();
  }
}

bool calSaveToFlash(void) {
  activeCal.magic = CAL_MAGIC;
  activeCal.version = CAL_VERSION;
  activeCal.checksum = calCalculateChecksum(&activeCal);
  return storageWrite(0, &activeCal, sizeof(CalConfig_t));
}

bool calLoadFromFlash(void) {
  CalConfig_t loaded;
  if (!storageRead(0, &loaded, sizeof(CalConfig_t))) {
    return false;
  }

  // Validate magic number, version, and integrity checksum
  if (loaded.magic != CAL_MAGIC || loaded.version != CAL_VERSION) {
    return false;
  }

  uint32_t expectedChecksum = calCalculateChecksum(&loaded);
  if (loaded.checksum != expectedChecksum) {
    return false;
  }

  // Validate point counts
  for (int i = 0; i < CAL_CH_COUNT; i++) {
    if (loaded.channels[i].count > CAL_MAX_POINTS) {
      return false;
    }
  }

  // Loaded successfully
  memcpy(&activeCal, &loaded, sizeof(CalConfig_t));
  if (activeCal.protection.runningTextEnabled > 1) {
    activeCal.protection.runningTextEnabled = 1;
  }
  return true;
}

uint16_t calInterpolate(CalChannelType ch, uint16_t rawInput) {
  if (ch >= CAL_CH_COUNT) return 0;
  const CalChannel_t *c = &activeCal.channels[ch];
  uint8_t size = c->count;
  if (size == 0) return 0;

  if (rawInput <= c->points[0].raw) {
    return c->points[0].value;
  }
  if (rawInput >= c->points[size - 1].raw) {
    return c->points[size - 1].value;
  }

  for (uint8_t i = 0; i < size - 1; i++) {
    if (rawInput >= c->points[i].raw && rawInput <= c->points[i + 1].raw) {
      uint32_t x0 = c->points[i].raw;
      uint32_t x1 = c->points[i + 1].raw;
      uint32_t y0 = c->points[i].value;
      uint32_t y1 = c->points[i + 1].value;

      if (x1 == x0) return (uint16_t)y0;
      return (uint16_t)(y0 + ((rawInput - x0) * (y1 - y0)) / (x1 - x0));
    }
  }
  return 0;
}

void calSortPoints(CalChannelType ch) {
  if (ch >= CAL_CH_COUNT) return;
  CalChannel_t *c = &activeCal.channels[ch];
  for (uint8_t i = 1; i < c->count; i++) {
    CalPoint_t key = c->points[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && c->points[j].raw > key.raw) {
      c->points[j + 1] = c->points[j];
      j--;
    }
    c->points[j + 1] = key;
  }
}

bool calCheckConflict(CalChannelType ch, uint16_t targetWatt, uint16_t rawADC, uint8_t *conflictIdx) {
  if (ch >= CAL_CH_COUNT) return false;
  const CalChannel_t *c = &activeCal.channels[ch];
  if (c->count == 0) return false;

  for (uint8_t i = 0; i < c->count; i++) {
    // If it's the exact same wattage, this is a recalibration replacement, not an inversion
    if (c->points[i].value == targetWatt) continue;

    // Inversion 1: lower wattage has higher or equal ADC
    if (c->points[i].value < targetWatt && c->points[i].raw >= rawADC) {
      if (conflictIdx) *conflictIdx = i;
      return true;
    }
    // Inversion 2: higher wattage has lower or equal ADC
    if (c->points[i].value > targetWatt && c->points[i].raw <= rawADC) {
      if (conflictIdx) *conflictIdx = i;
      return true;
    }
  }
  return false;
}

bool calAdaptAllPoints(CalChannelType ch, uint16_t targetWatt, uint16_t newRawADC) {
  if (ch >= CAL_CH_COUNT) return false;
  CalChannel_t *c = &activeCal.channels[ch];
  if (c->count == 0) {
    return calAddPoint(ch, targetWatt, newRawADC);
  }

  if (targetWatt > c->maxWatts) targetWatt = c->maxWatts;
  if (newRawADC > 4095) newRawADC = 4095;
  if (newRawADC == 0) newRawADC = 1;

  // 1. Determine baseline ADC for targetWatt on current curve
  uint32_t baseADC = 0;
  int8_t existingIdx = -1;

  for (uint8_t i = 0; i < c->count; i++) {
    if (c->points[i].value == targetWatt) {
      existingIdx = (int8_t)i;
      baseADC = c->points[i].raw;
      break;
    }
  }

  if (existingIdx < 0) {
    // Target watt is not an existing point: interpolate baseADC from current curve
    if (targetWatt <= c->points[0].value) {
      uint32_t p0_val = c->points[0].value ? c->points[0].value : 1;
      baseADC = ((uint32_t)targetWatt * c->points[0].raw) / p0_val;
    } else if (targetWatt >= c->points[c->count - 1].value) {
      uint32_t pLast_val = c->points[c->count - 1].value ? c->points[c->count - 1].value : 1;
      baseADC = ((uint32_t)targetWatt * c->points[c->count - 1].raw) / pLast_val;
    } else {
      for (uint8_t i = 0; i < c->count - 1; i++) {
        if (targetWatt >= c->points[i].value && targetWatt <= c->points[i + 1].value) {
          uint32_t y0 = c->points[i].raw;
          uint32_t y1 = c->points[i + 1].raw;
          uint32_t x0 = c->points[i].value;
          uint32_t x1 = c->points[i + 1].value;
          if (x1 > x0) {
            baseADC = y0 + ((targetWatt - x0) * (y1 - y0)) / (x1 - x0);
          } else {
            baseADC = y0;
          }
          break;
        }
      }
    }
  }

  if (baseADC == 0) baseADC = 1;

  // 2. Scale all points proportionally
  for (uint8_t i = 0; i < c->count; i++) {
    if (c->points[i].value == targetWatt) {
      c->points[i].raw = newRawADC;
    } else {
      uint32_t scaled = ((uint32_t)c->points[i].raw * newRawADC + (baseADC / 2)) / baseADC;
      if (scaled > 4095) scaled = 4095;
      if (scaled == 0 && c->points[i].value > 0) scaled = 1;
      c->points[i].raw = (uint16_t)scaled;
    }
  }

  // 3. If targetWatt was new, add it
  if (existingIdx < 0 && c->count < CAL_MAX_POINTS) {
    c->points[c->count].raw = newRawADC;
    c->points[c->count].value = targetWatt;
    c->count++;
  }

  // 4. Sort and guarantee strict monotonicity
  calSortPoints(ch);
  for (uint8_t i = 0; i < c->count - 1; i++) {
    if (c->points[i + 1].raw <= c->points[i].raw) {
      c->points[i + 1].raw = (c->points[i].raw < 4095) ? c->points[i].raw + 1 : 4095;
    }
  }

  return true;
}

bool calAddPoint(CalChannelType ch, uint16_t targetWatt, uint16_t rawADC) {
  if (ch >= CAL_CH_COUNT) return false;
  CalChannel_t *c = &activeCal.channels[ch];

  if (targetWatt > c->maxWatts) targetWatt = c->maxWatts;
  if (rawADC > 4095) rawADC = 4095;

  // If a point with this exact target watt already exists, recalibrate that point!
  for (uint8_t i = 0; i < c->count; i++) {
    if (c->points[i].value == targetWatt) {
      c->points[i].raw = rawADC;
      calSortPoints(ch);
      return true;
    }
  }

  if (c->count >= CAL_MAX_POINTS) return false; // Channel full

  uint8_t idx = c->count;
  c->points[idx].raw = rawADC;
  c->points[idx].value = targetWatt;
  c->count++;
  calSortPoints(ch);
  return true;
}

bool calUpdatePoint(CalChannelType ch, uint8_t pointIndex, uint16_t targetWatt, uint16_t rawADC) {
  if (ch >= CAL_CH_COUNT) return false;
  CalChannel_t *c = &activeCal.channels[ch];
  if (pointIndex >= c->count) return false;

  if (targetWatt > c->maxWatts) targetWatt = c->maxWatts;
  if (rawADC > 4095) rawADC = 4095;

  c->points[pointIndex].raw = rawADC;
  c->points[pointIndex].value = targetWatt;
  calSortPoints(ch);
  return true;
}

bool calRemovePoint(CalChannelType ch, uint8_t pointIndex) {
  if (ch >= CAL_CH_COUNT) return false;
  CalChannel_t *c = &activeCal.channels[ch];
  if (c->count == 0) return false;
  if (pointIndex >= c->count) return false;

  for (uint8_t i = pointIndex; i < c->count - 1; i++) {
    c->points[i] = c->points[i + 1];
  }
  c->points[c->count - 1].raw = 0;
  c->points[c->count - 1].value = 0;
  c->count--;
  return true;
}

bool calRemoveAllPoints(CalChannelType ch) {
  if (ch >= CAL_CH_COUNT) return false;
  CalChannel_t *c = &activeCal.channels[ch];
  c->count = 0;
  memset(c->points, 0, sizeof(c->points));
  return true;
}

uint8_t calGetPointCount(CalChannelType ch) {
  if (ch >= CAL_CH_COUNT) return 0;
  return activeCal.channels[ch].count;
}

const char* calGetChannelName(CalChannelType ch) {
  switch (ch) {
    case CAL_CH_FWD: return "FWD";
    case CAL_CH_REF: return "REF";
    case CAL_CH_RAD: return "RAD";
    default: return "???";
  }
}

uint16_t calGetChannelMaxWatts(CalChannelType ch) {
  if (ch >= CAL_CH_COUNT) return 1000;
  return activeCal.channels[ch].maxWatts;
}

void calSetChannelMaxWatts(CalChannelType ch, uint16_t maxW) {
  if (ch < CAL_CH_COUNT) {
    activeCal.channels[ch].maxWatts = maxW;
  }
}
