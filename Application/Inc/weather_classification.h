#ifndef WEATHER_CLASSIFICATION_H
#define WEATHER_CLASSIFICATION_H
#include "weather_data.h"

typedef enum {
  WEATHER_UNKNOWN = 0, WEATHER_NORMAL, WEATHER_UNFAVORABLE, WEATHER_SEVERE
} WeatherStatus;
enum {
  WEATHER_REASON_HOT = 1, WEATHER_REASON_COLD = 2,
  WEATHER_REASON_HUMID = 4, WEATHER_REASON_RAIN = 8,
  WEATHER_REASON_MISSING = 16, WEATHER_REASON_INVALID = 32,
  WEATHER_REASON_STARTUP = 64
};
typedef struct {
  uint8_t status, reasons, pending, count;
  uint8_t hot, cold, humid;
  uint8_t sampled;
  uint32_t last_tick;
} WeatherClassifier;

void WeatherClassifier_Init(WeatherClassifier *state);
void WeatherClassifier_Update(WeatherClassifier *state, WeatherData *data, uint32_t tick);

/* Header-only labels keep CSV formatting independent of classifier state. */
static inline const char *WeatherStatus_Name(uint8_t status) {
  switch (status) {
    case WEATHER_NORMAL: return "NORMAL";
    case WEATHER_UNFAVORABLE: return "UNFAVORABLE";
    case WEATHER_SEVERE: return "SEVERE";
    default: return "UNKNOWN";
  }
}
static inline const char *WeatherReason_Primary(uint8_t reasons) {
  if (reasons & WEATHER_REASON_MISSING) return "SENSOR MISSING";
  if (reasons & WEATHER_REASON_INVALID) return "SENSOR INVALID";
  if (reasons & WEATHER_REASON_STARTUP) return "CHECKING";
  if (reasons & WEATHER_REASON_HOT) return "HIGH TEMP";
  if (reasons & WEATHER_REASON_COLD) return "LOW TEMP";
  if (reasons & WEATHER_REASON_HUMID) return "HIGH HUMIDITY";
  if (reasons & WEATHER_REASON_RAIN) return "RAIN DETECTED";
  return "WITHIN LIMITS";
}
#endif
