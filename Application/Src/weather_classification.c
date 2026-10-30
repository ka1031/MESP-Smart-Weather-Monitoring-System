#include "weather_classification.h"
#include "app_config.h"
#include <math.h>

void WeatherClassifier_Init(WeatherClassifier *s)
{
  *s = (WeatherClassifier){.reasons = WEATHER_REASON_STARTUP};
}

/* Independent hysteresis for each threshold; 0=normal, 1=warning, 2=severe. */
static uint8_t HighLevel(float value, float warn, float severe, float hysteresis,
                         uint8_t previous)
{
  if (value >= severe || (previous == 2U && value > severe - hysteresis)) return 2U;
  if (value >= warn || (previous != 0U && value > warn - hysteresis)) return 1U;
  return 0U;
}

void WeatherClassifier_Update(WeatherClassifier *s, WeatherData *d, uint32_t tick)
{
  /* A long sampling interruption cannot count as consecutive good samples. */
  if (s->sampled && (uint32_t)(tick - s->last_tick) > APP_CONFIG_WEATHER_MAX_GAP_MS)
    WeatherClassifier_Init(s);
  s->last_tick = tick;
  s->sampled = 1U;
  uint8_t fault = 0U;
  const uint32_t required = WEATHER_DATA_VALID_BME280 | WEATHER_DATA_VALID_RAIN;
  if ((d->valid_flags & required) != required) fault = WEATHER_REASON_MISSING;
  else if (!isfinite(d->temperature_c) || d->temperature_c < -40.0F || d->temperature_c > 85.0F ||
           !isfinite(d->humidity_percent) || d->humidity_percent < 0.0F || d->humidity_percent > 100.0F ||
           d->rain_wet > 1U || d->rain_raw > 4095U || d->rain_percent > 100U)
    fault = WEATHER_REASON_INVALID;
  if (fault) {
    WeatherClassifier_Init(s);
    s->reasons = fault;
  } else {
    s->hot = HighLevel(d->temperature_c, APP_CONFIG_WEATHER_HOT_WARN_C,
        APP_CONFIG_WEATHER_HOT_SEVERE_C, APP_CONFIG_WEATHER_TEMP_HYST_C, s->hot);
    s->cold = HighLevel(-d->temperature_c, -APP_CONFIG_WEATHER_COLD_WARN_C,
        -APP_CONFIG_WEATHER_COLD_SEVERE_C, APP_CONFIG_WEATHER_TEMP_HYST_C, s->cold);
    s->humid = (d->humidity_percent >= APP_CONFIG_WEATHER_HUMID_WARN_PCT ||
        (s->humid && d->humidity_percent > APP_CONFIG_WEATHER_HUMID_WARN_PCT -
                                                    APP_CONFIG_WEATHER_HUMID_HYST_PCT));
    uint8_t reasons = (s->hot ? WEATHER_REASON_HOT : 0U) |
        (s->cold ? WEATHER_REASON_COLD : 0U) |
        (s->humid ? WEATHER_REASON_HUMID : 0U) |
        (d->rain_wet ? WEATHER_REASON_RAIN : 0U);
    uint8_t next = (s->hot == 2U || s->cold == 2U) ? WEATHER_SEVERE :
        reasons ? WEATHER_UNFAVORABLE : WEATHER_NORMAL;
    if (next == s->status) {
      s->count = 0U;
      s->pending = next;
      s->reasons = reasons;
    } else {
      if (s->pending != next) { s->pending = next; s->count = 0U; }
      if (s->count < 255U) ++s->count;
      uint32_t needed = next < s->status ? APP_CONFIG_WEATHER_CLEAR_SAMPLES :
                                          APP_CONFIG_WEATHER_CONFIRM_SAMPLES;
      if (s->count >= needed) {
        s->status = next; s->reasons = reasons; s->count = 0U;
      }
      /* Until confirmation, keep the previous status and its reasons. */
    }
  }
  d->weather_status = s->status;
  d->weather_reasons = s->reasons;
}
