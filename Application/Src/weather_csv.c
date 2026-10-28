#include "weather_csv.h"
#include <math.h>
#include <stdio.h>

const char WEATHER_CSV_HEADER[] =
    "uptime_ms,timestamp,time_valid,temperature_c,humidity_pct,pressure_hpa,"
    "light_lux,rain_raw,rain_pct,rain_wet,valid_flags\r\n";

/* Integer formatting avoids pulling newlib's large floating printf into flash. */
static void Decimal(char output[20], float value, uint8_t valid)
{
  output[0] = '\0';
  if (!valid || !isfinite(value) || value < -1000000.0F || value > 1000000.0F)
    return;
  int32_t scaled = (int32_t)(value * 100.0F + (value < 0.0F ? -0.5F : 0.5F));
  uint32_t magnitude = (uint32_t)(scaled < 0 ? -scaled : scaled);
  (void)snprintf(output, 20U, "%s%lu.%02lu", scaled < 0 ? "-" : "",
      (unsigned long)(magnitude / 100U), (unsigned long)(magnitude % 100U));
}

size_t WeatherCSV_Format(char *buffer, size_t capacity,
                         const WeatherData *data, uint32_t sample_tick)
{
  char timestamp[32] = "", temperature[20], humidity[20], pressure[20], light[20];
  char rain_raw[8] = "", rain_pct[8] = "", rain_wet[4] = "";
  const uint8_t time_valid = (data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP) != 0U;
  const uint8_t bme_valid = (data->valid_flags & WEATHER_DATA_VALID_BME280) != 0U;
  if (time_valid)
  {
    const WeatherTimestamp *t = &data->timestamp;
    (void)snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u",
        (unsigned)t->year, (unsigned)t->month, (unsigned)t->day,
        (unsigned)t->hour, (unsigned)t->minute, (unsigned)t->second);
  }
  Decimal(temperature, data->temperature_c, bme_valid);
  Decimal(humidity, data->humidity_percent, bme_valid);
  Decimal(pressure, data->pressure_hpa, bme_valid);
  Decimal(light, data->illuminance_lux,
      (data->valid_flags & WEATHER_DATA_VALID_BH1750) != 0U);
  if ((data->valid_flags & WEATHER_DATA_VALID_RAIN) != 0U)
  {
    (void)snprintf(rain_raw, sizeof(rain_raw), "%u", (unsigned)data->rain_raw);
    (void)snprintf(rain_pct, sizeof(rain_pct), "%u", (unsigned)data->rain_percent);
    (void)snprintf(rain_wet, sizeof(rain_wet), "%u", (unsigned)(data->rain_wet != 0U));
  }
  int length = snprintf(buffer, capacity, "%lu,%s,%u,%s,%s,%s,%s,%s,%s,%s,%lu\r\n",
      (unsigned long)sample_tick, timestamp, (unsigned)time_valid,
      temperature, humidity, pressure, light, rain_raw, rain_pct, rain_wet,
      (unsigned long)data->valid_flags);
  return (length > 0 && (size_t)length < capacity) ? (size_t)length : 0U;
}
