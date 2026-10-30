#include "weather_classification.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static WeatherClassifier c;
static WeatherData d;
static uint32_t tick;
static void sample(unsigned count) {
  while (count--) { WeatherClassifier_Update(&c, &d, tick); tick += 1000U; }
}
static void reset(void) {
  WeatherClassifier_Init(&c);
  d = (WeatherData){.temperature_c=25, .humidity_percent=60,
      .rain_raw=3000, .rain_percent=25, .valid_flags=5};
  tick=0;
}
int main(void) {
  reset(); sample(2); assert(d.weather_status == WEATHER_UNKNOWN);
  sample(1); assert(d.weather_status == WEATHER_NORMAL && !d.weather_reasons);
  d.temperature_c=35; sample(2); assert(d.weather_status == WEATHER_NORMAL);
  sample(1); assert(d.weather_status == WEATHER_UNFAVORABLE && d.weather_reasons == WEATHER_REASON_HOT);
  d.temperature_c=34.5F; sample(20); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.temperature_c=34; sample(9); assert(d.weather_status == WEATHER_UNFAVORABLE);
  sample(1); assert(d.weather_status == WEATHER_NORMAL);
  d.temperature_c=40; sample(3); assert(d.weather_status == WEATHER_SEVERE);
  d.temperature_c=39.5F; sample(10); assert(d.weather_status == WEATHER_SEVERE);
  d.temperature_c=39; sample(9); assert(d.weather_status == WEATHER_SEVERE);
  sample(1); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.temperature_c=25; sample(10); assert(d.weather_status == WEATHER_NORMAL);
  d.temperature_c=5; sample(3); assert(d.weather_status == WEATHER_SEVERE && d.weather_reasons == WEATHER_REASON_COLD);
  d.temperature_c=6; sample(10); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.temperature_c=10.5F; sample(10); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.temperature_c=11; sample(10); assert(d.weather_status == WEATHER_NORMAL);
  d.humidity_percent=85; d.rain_wet=1; sample(3);
  assert(d.weather_status == WEATHER_UNFAVORABLE && d.weather_reasons == (WEATHER_REASON_HUMID|WEATHER_REASON_RAIN));
  d.humidity_percent=84; d.rain_wet=0; sample(10); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.humidity_percent=80; sample(10); assert(d.weather_status == WEATHER_NORMAL);
  d.rain_wet=1; sample(2); d.rain_wet=0; sample(1); d.rain_wet=1; sample(2);
  assert(d.weather_status == WEATHER_NORMAL); sample(1); assert(d.weather_status == WEATHER_UNFAVORABLE);
  d.valid_flags=4; sample(1); assert(d.weather_status == WEATHER_UNKNOWN && d.weather_reasons == WEATHER_REASON_MISSING);
  d.valid_flags=5; d.rain_wet=0; sample(2); assert(d.weather_status == WEATHER_UNKNOWN);
  sample(1); assert(d.weather_status == WEATHER_NORMAL);
  d.temperature_c=NAN; sample(1); assert(d.weather_status == WEATHER_UNKNOWN && d.weather_reasons == WEATHER_REASON_INVALID);
  d.temperature_c=INFINITY; sample(1); assert(d.weather_status == WEATHER_UNKNOWN);
  d.temperature_c=25; d.humidity_percent=101; sample(1); assert(d.weather_reasons == WEATHER_REASON_INVALID);
  d.humidity_percent=60; d.rain_raw=4096; sample(1); assert(d.weather_reasons == WEATHER_REASON_INVALID);
  d.rain_raw=3000; d.rain_wet=2; sample(1); assert(d.weather_reasons == WEATHER_REASON_INVALID);
  reset(); sample(3); tick+=6000; sample(1); assert(d.weather_status == WEATHER_UNKNOWN);
  sample(2); assert(d.weather_status == WEATHER_NORMAL);
  reset(); tick=UINT32_MAX-1500U; sample(3); assert(d.weather_status == WEATHER_NORMAL);
  // Light, pressure, RTC and wind are not classification inputs.
  d.illuminance_lux=0; d.pressure_hpa=0; sample(10); assert(d.weather_status == WEATHER_NORMAL);
  puts("Classification: thresholds, hysteresis, confirmation, recovery, faults, gaps and rollover passed");
}
