#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

#include <stdint.h>

typedef enum
{
  WEATHER_DATA_VALID_NONE       = 0U,
  WEATHER_DATA_VALID_BME280     = (1U << 0),
  WEATHER_DATA_VALID_BH1750     = (1U << 1),
  WEATHER_DATA_VALID_RAIN       = (1U << 2),
  WEATHER_DATA_VALID_TIMESTAMP  = (1U << 3)
} WeatherDataValidFlags;

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} WeatherTimestamp;

typedef struct
{
  float temperature_c;
  float humidity_percent;
  float pressure_hpa;
  float illuminance_lux;
  uint16_t rain_raw;
  uint8_t rain_percent;
  uint8_t rain_wet;
  WeatherTimestamp timestamp;
  uint32_t valid_flags;
} WeatherData;

#endif /* WEATHER_DATA_H */
