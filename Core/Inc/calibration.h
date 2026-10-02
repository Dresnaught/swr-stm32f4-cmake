#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

#define CAL_MAGIC           0x53575232U  // 'SWR2'
#define CAL_VERSION         2U
#define CAL_MAX_POINTS      10           // Max capacity per channel
#define CAL_MIN_POINTS      2            // Minimum points required for interpolation
#define CAL_DEFAULT_POINTS  7            // Default count from factory curves
#define CAL_SAMPLE_SECONDS  5            // 5-second sampling window

typedef enum {
  CAL_CH_FWD = 0,   // Forward Power (Max: 1000W)
  CAL_CH_REF = 1,   // Reflected Power (Max: 100W)
  CAL_CH_RAD = 2,   // Radio-In / Drive Power (Max: 50W)
  CAL_CH_COUNT = 3
} CalChannelType;

typedef struct {
  uint16_t raw;     // Raw ADC value (0 - 4095)
  uint16_t value;   // Power in Watts
} CalPoint_t;

typedef struct {
  uint8_t count;               // Active points count (2..CAL_MAX_POINTS)
  uint8_t reserved;
  uint16_t maxWatts;           // Channel maximum scale in Watts
  CalPoint_t points[CAL_MAX_POINTS];
} CalChannel_t;

typedef enum {
  BAR_STYLE_OFF = 0,    // 0: Off (Row 1 shows clean text: REF: %4uW or RAD: %4uW)
  BAR_STYLE_SCALE,      // 1: Scale bar: ''''|<-half ''''|<-full
  BAR_STYLE_PIPES,      // 2: Continuous bar: ||||| per pixel
  BAR_STYLE_COUNT
} PowerBarStyle_t;

typedef struct {
  uint16_t radLimitWatts; // 1 to 50W (default: 15W)
  uint16_t swrLimitX100;  // 110 to 500 (default: 200 for 2.0 SWR)
  uint16_t enabled;       // 1 = enabled, 0 = disabled
  uint8_t barStyle;       // Power bar style (PowerBarStyle_t)
  uint8_t runningTextEnabled; // 1 = Running text marquee enabled in standby, 0 = disabled
} ProtectionSettings_t;

typedef struct {
  uint32_t magic;
  uint32_t version;
  CalChannel_t channels[CAL_CH_COUNT];
  ProtectionSettings_t protection;
  uint32_t checksum;
} CalConfig_t;

extern CalConfig_t activeCal;

// Portable lifecycle & persistence functions
void calInit(void);
void calResetDefaults(void);
bool calSaveToFlash(void);
bool calLoadFromFlash(void);

// Piecewise linear interpolation
uint16_t calInterpolate(CalChannelType ch, uint16_t rawInput);

// Dynamic Point management (Add, Edit, Remove)
bool calAddPoint(CalChannelType ch, uint16_t targetWatt, uint16_t rawADC);
bool calUpdatePoint(CalChannelType ch, uint8_t pointIndex, uint16_t targetWatt, uint16_t rawADC);
bool calRemovePoint(CalChannelType ch, uint8_t pointIndex);
bool calRemoveAllPoints(CalChannelType ch);
void calSortPoints(CalChannelType ch);
uint8_t calGetPointCount(CalChannelType ch);

// Monotonicity check & proportional curve adaptation
bool calCheckConflict(CalChannelType ch, uint16_t targetWatt, uint16_t rawADC, uint8_t *conflictIdx);
bool calAdaptAllPoints(CalChannelType ch, uint16_t targetWatt, uint16_t newRawADC);

// Channel metadata helpers
const char* calGetChannelName(CalChannelType ch);
uint16_t calGetChannelMaxWatts(CalChannelType ch);
void calSetChannelMaxWatts(CalChannelType ch, uint16_t maxW);

#endif // CALIBRATION_H
