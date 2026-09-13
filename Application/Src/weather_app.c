#include "weather_app.h"

#include "app_log.h"

#include <stddef.h>

static I2C_HandleTypeDef *weather_sensor_bus;
static WeatherData latest_weather_data;

void WeatherApp_Init(I2C_HandleTypeDef *sensor_bus,
                     UART_HandleTypeDef *debug_uart)
{
  weather_sensor_bus = sensor_bus;
  AppLog_Init(debug_uart);
  latest_weather_data.valid_flags = WEATHER_DATA_VALID_NONE;
}

void WeatherApp_Run(void)
{
  /* Sensor scheduling will be added one module at a time. */
  (void)weather_sensor_bus;
}

const WeatherData *WeatherApp_GetLatestData(void)
{
  return &latest_weather_data;
}
