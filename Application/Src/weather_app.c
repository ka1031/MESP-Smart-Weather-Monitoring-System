#include "weather_app.h"

#include "app_config.h"
#include "app_log.h"
#include "bh1750.h"
#include "bme280.h"
#include "rain_sensor.h"
#include "ssd1306.h"

#if APP_CONFIG_ENABLE_I2C_SCANNER != 0U
#include "temporary_i2c_scanner.h"
#endif

#include <stddef.h>
#include <stdio.h>

static WeatherData latest_weather_data;
static BME280_Handle bme280;
static BH1750_Handle bh1750;
static RainSensor_Handle rain_sensor;
static SSD1306_Handle oled;

static DeviceStatus bme280_status = DEVICE_STATUS_NOT_INITIALIZED;
static DeviceStatus bh1750_status = DEVICE_STATUS_NOT_INITIALIZED;
static DeviceStatus rain_status = DEVICE_STATUS_NOT_INITIALIZED;
static DeviceStatus oled_status = DEVICE_STATUS_NOT_INITIALIZED;
static uint32_t last_sample_tick;

static int32_t WeatherApp_ToHundredths(float value)
{
  float scaled = value * 100.0F;
  return (int32_t)(scaled + ((scaled >= 0.0F) ? 0.5F : -0.5F));
}

static void WeatherApp_LogStatus(const char *device_name,
                                 const char *operation,
                                 const char *status_text)
{
  char message[96];

  (void)snprintf(message,
                 sizeof(message),
                 "%s %s failed: %s\r\n",
                 device_name,
                 operation,
                 status_text);
  (void)AppLog_Write(message);
}

static DeviceStatus WeatherApp_InitBme280(I2C_HandleTypeDef *sensor_bus)
{
  DeviceStatus primary_status;
  DeviceStatus secondary_status;

  primary_status = BME280_Init(&bme280,
                               sensor_bus,
                               APP_CONFIG_BME280_ADDRESS_PRIMARY);
  if (primary_status == DEVICE_STATUS_OK)
  {
    return DEVICE_STATUS_OK;
  }

  secondary_status = BME280_Init(&bme280,
                                 sensor_bus,
                                 APP_CONFIG_BME280_ADDRESS_SECONDARY);
  if (secondary_status == DEVICE_STATUS_OK)
  {
    return DEVICE_STATUS_OK;
  }

  if (primary_status != DEVICE_STATUS_NOT_FOUND)
  {
    return primary_status;
  }
  return secondary_status;
}

static DeviceStatus WeatherApp_InitBh1750(I2C_HandleTypeDef *sensor_bus)
{
  DeviceStatus primary_status;
  DeviceStatus secondary_status;

  primary_status = BH1750_Init(&bh1750,
                               sensor_bus,
                               APP_CONFIG_BH1750_ADDRESS_PRIMARY);
  if (primary_status == DEVICE_STATUS_OK)
  {
    return DEVICE_STATUS_OK;
  }

  secondary_status = BH1750_Init(&bh1750,
                                 sensor_bus,
                                 APP_CONFIG_BH1750_ADDRESS_SECONDARY);
  if (secondary_status == DEVICE_STATUS_OK)
  {
    return DEVICE_STATUS_OK;
  }

  if (primary_status != DEVICE_STATUS_NOT_FOUND)
  {
    return primary_status;
  }
  return secondary_status;
}

static DeviceStatus WeatherApp_InitRainSensor(ADC_HandleTypeDef *rain_adc)
{
  RainSensor_Config config;

  config.digital_port = APP_CONFIG_RAIN_DIGITAL_PORT;
  config.digital_pin = APP_CONFIG_RAIN_DIGITAL_PIN;
  config.dry_raw = APP_CONFIG_RAIN_DRY_RAW;
  config.wet_raw = APP_CONFIG_RAIN_WET_RAW;
  config.adc_timeout_ms = APP_CONFIG_RAIN_ADC_TIMEOUT_MS;
  config.digital_active_low = APP_CONFIG_RAIN_DIGITAL_ACTIVE_LOW;
  config.sample_count = APP_CONFIG_RAIN_ADC_SAMPLE_COUNT;

  return RainSensor_Init(&rain_sensor, rain_adc, &config);
}

static void WeatherApp_LogInitializedAddress(const char *device_name,
                                             uint8_t address_7bit)
{
  char message[72];

  (void)snprintf(message,
                 sizeof(message),
                 "%s initialized at 7-bit address 0x%02X\r\n",
                 device_name,
                 address_7bit);
  (void)AppLog_Write(message);
}

static void WeatherApp_PrintBme280Measurement(
    const BME280_Measurement *measurement)
{
  char message[112];
  int32_t temperature = WeatherApp_ToHundredths(measurement->temperature_c);
  uint32_t temperature_magnitude = (temperature < 0)
      ? (uint32_t)(-(int64_t)temperature)
      : (uint32_t)temperature;
  uint32_t humidity = (uint32_t)WeatherApp_ToHundredths(
      measurement->humidity_percent);
  uint32_t pressure = (uint32_t)WeatherApp_ToHundredths(
      measurement->pressure_hpa);

  (void)snprintf(message,
                 sizeof(message),
                 "BME280: T=%s%lu.%02lu C, RH=%lu.%02lu %%, P=%lu.%02lu hPa\r\n",
                 (temperature < 0) ? "-" : "",
                 (unsigned long)(temperature_magnitude / 100U),
                 (unsigned long)(temperature_magnitude % 100U),
                 (unsigned long)(humidity / 100U),
                 (unsigned long)(humidity % 100U),
                 (unsigned long)(pressure / 100U),
                 (unsigned long)(pressure % 100U));
  (void)AppLog_Write(message);
}

static void WeatherApp_PrintBh1750Measurement(float illuminance_lux)
{
  char message[64];
  uint32_t lux = (uint32_t)WeatherApp_ToHundredths(illuminance_lux);

  (void)snprintf(message,
                 sizeof(message),
                 "BH1750: Light=%lu.%02lu lux\r\n",
                 (unsigned long)(lux / 100U),
                 (unsigned long)(lux % 100U));
  (void)AppLog_Write(message);
}

static void WeatherApp_PrintRainMeasurement(
    const RainSensor_Measurement *measurement)
{
  char message[72];

  (void)snprintf(message,
                 sizeof(message),
                 "Rain: %s, raw=%u, approximate=%u%%\r\n",
                 (measurement->wet != 0U) ? "WET" : "DRY",
                 (unsigned int)measurement->raw,
                 (unsigned int)measurement->percent);
  (void)AppLog_Write(message);
}

static void WeatherApp_WriteDisplayLine(uint8_t page, const char *text)
{
  SSD1306_SetCursor(&oled, 4U, page);
  SSD1306_WriteString(&oled, text);
}

static void WeatherApp_UpdateDisplay(void)
{
  char line[24];
  int32_t value;
  uint32_t magnitude;
  DeviceStatus update_status;

  if ((APP_CONFIG_ENABLE_OLED == 0U) || (oled.initialized == 0U))
  {
    return;
  }

  SSD1306_Clear(&oled);
  WeatherApp_WriteDisplayLine(0U, "WEATHER STATION");

  if ((latest_weather_data.valid_flags & WEATHER_DATA_VALID_BME280) != 0U)
  {
    value = WeatherApp_ToHundredths(latest_weather_data.temperature_c);
    magnitude = (value < 0) ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    (void)snprintf(line,
                   sizeof(line),
                   "TEMP %s%lu.%02lu C",
                   (value < 0) ? "-" : "",
                   (unsigned long)(magnitude / 100U),
                   (unsigned long)(magnitude % 100U));
    WeatherApp_WriteDisplayLine(1U, line);

    magnitude = (uint32_t)WeatherApp_ToHundredths(
        latest_weather_data.humidity_percent);
    (void)snprintf(line,
                   sizeof(line),
                   "HUM  %lu.%02lu PCT",
                   (unsigned long)(magnitude / 100U),
                   (unsigned long)(magnitude % 100U));
    WeatherApp_WriteDisplayLine(2U, line);

    magnitude = (uint32_t)WeatherApp_ToHundredths(
        latest_weather_data.pressure_hpa);
    (void)snprintf(line,
                   sizeof(line),
                   "PRES %lu.%02lu HPA",
                   (unsigned long)(magnitude / 100U),
                   (unsigned long)(magnitude % 100U));
    WeatherApp_WriteDisplayLine(3U, line);
  }
  else
  {
    WeatherApp_WriteDisplayLine(1U, "BME280 ERROR");
  }

  if ((latest_weather_data.valid_flags & WEATHER_DATA_VALID_BH1750) != 0U)
  {
    magnitude = (uint32_t)WeatherApp_ToHundredths(
        latest_weather_data.illuminance_lux);
    (void)snprintf(line,
                   sizeof(line),
                   "LIGHT %lu.%02lu LUX",
                   (unsigned long)(magnitude / 100U),
                   (unsigned long)(magnitude % 100U));
    WeatherApp_WriteDisplayLine(4U, line);
  }
  else
  {
    WeatherApp_WriteDisplayLine(4U, "BH1750 ERROR");
  }

  if ((latest_weather_data.valid_flags & WEATHER_DATA_VALID_RAIN) != 0U)
  {
    (void)snprintf(line,
                   sizeof(line),
                   "RAIN %s %u",
                   (latest_weather_data.rain_wet != 0U) ? "WET" : "DRY",
                   (unsigned int)latest_weather_data.rain_raw);
    WeatherApp_WriteDisplayLine(6U, line);

    (void)snprintf(line,
                   sizeof(line),
                   "RAIN LEVEL %u PCT",
                   (unsigned int)latest_weather_data.rain_percent);
    WeatherApp_WriteDisplayLine(7U, line);
  }
  else
  {
    WeatherApp_WriteDisplayLine(6U, "RAIN SENSOR ERROR");
  }

  update_status = SSD1306_UpdateScreen(&oled);
  oled_status = update_status;
  if (update_status != DEVICE_STATUS_OK)
  {
    WeatherApp_LogStatus("OLED",
                         "update",
                         SSD1306_StatusString(update_status));
  }
}

static void WeatherApp_ReadBme280(void)
{
  BME280_Measurement measurement;
  DeviceStatus status;

  if ((APP_CONFIG_ENABLE_BME280 == 0U) ||
      (bme280_status != DEVICE_STATUS_OK))
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_BME280;
    return;
  }

  status = BME280_Read(&bme280, &measurement);
  if (status != DEVICE_STATUS_OK)
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_BME280;
    WeatherApp_LogStatus("BME280", "read", BME280_StatusString(status));
    return;
  }

  latest_weather_data.temperature_c = measurement.temperature_c;
  latest_weather_data.humidity_percent = measurement.humidity_percent;
  latest_weather_data.pressure_hpa = measurement.pressure_hpa;
  latest_weather_data.valid_flags |= WEATHER_DATA_VALID_BME280;
  WeatherApp_PrintBme280Measurement(&measurement);
}

static void WeatherApp_ReadBh1750(void)
{
  float illuminance_lux;
  DeviceStatus status;

  if ((APP_CONFIG_ENABLE_BH1750 == 0U) ||
      (bh1750_status != DEVICE_STATUS_OK))
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_BH1750;
    return;
  }

  status = BH1750_ReadLux(&bh1750, &illuminance_lux);
  if (status != DEVICE_STATUS_OK)
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_BH1750;
    WeatherApp_LogStatus("BH1750", "read", BH1750_StatusString(status));
    return;
  }

  latest_weather_data.illuminance_lux = illuminance_lux;
  latest_weather_data.valid_flags |= WEATHER_DATA_VALID_BH1750;
  WeatherApp_PrintBh1750Measurement(illuminance_lux);
}

static void WeatherApp_ReadRainSensor(void)
{
  RainSensor_Measurement measurement;
  DeviceStatus status;

  if ((APP_CONFIG_ENABLE_RAIN_SENSOR == 0U) ||
      (rain_status != DEVICE_STATUS_OK))
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_RAIN;
    return;
  }

  status = RainSensor_Read(&rain_sensor, &measurement);
  if (status != DEVICE_STATUS_OK)
  {
    latest_weather_data.valid_flags &= ~WEATHER_DATA_VALID_RAIN;
    WeatherApp_LogStatus("Rain sensor",
                         "read",
                         RainSensor_StatusString(status));
    return;
  }

  latest_weather_data.rain_raw = measurement.raw;
  latest_weather_data.rain_percent = measurement.percent;
  latest_weather_data.rain_wet = measurement.wet;
  latest_weather_data.valid_flags |= WEATHER_DATA_VALID_RAIN;
  WeatherApp_PrintRainMeasurement(&measurement);
}

void WeatherApp_Init(I2C_HandleTypeDef *sensor_bus,
                     ADC_HandleTypeDef *rain_adc,
                     UART_HandleTypeDef *debug_uart)
{
  AppLog_Init(debug_uart);
  latest_weather_data = (WeatherData){0};

  if ((sensor_bus == NULL) || (rain_adc == NULL) || (debug_uart == NULL))
  {
    (void)AppLog_Write("Weather application initialization failed: invalid HAL handle\r\n");
    return;
  }

  (void)AppLog_Write("\r\nMESP weather station starting\r\n");

#if APP_CONFIG_ENABLE_I2C_SCANNER != 0U
  /* TEMPORARY: enable only while diagnosing the physical I2C1 bus. */
  (void)TemporaryI2CScanner_Run(sensor_bus);
#endif

  if (APP_CONFIG_ENABLE_OLED != 0U)
  {
    oled_status = SSD1306_Init(&oled,
                               sensor_bus,
                               APP_CONFIG_OLED_ADDRESS_PRIMARY,
                               APP_CONFIG_OLED_ADDRESS_SECONDARY);
    if (oled_status == DEVICE_STATUS_OK)
    {
      WeatherApp_LogInitializedAddress("OLED", oled.address_7bit);
    }
    else
    {
      WeatherApp_LogStatus("OLED",
                           "initialization",
                           SSD1306_StatusString(oled_status));
    }
  }

  if (APP_CONFIG_ENABLE_BME280 != 0U)
  {
    bme280_status = WeatherApp_InitBme280(sensor_bus);
    if (bme280_status == DEVICE_STATUS_OK)
    {
      WeatherApp_LogInitializedAddress("BME280", bme280.address_7bit);
    }
    else
    {
      WeatherApp_LogStatus("BME280",
                           "initialization",
                           BME280_StatusString(bme280_status));
    }
  }

  if (APP_CONFIG_ENABLE_BH1750 != 0U)
  {
    bh1750_status = WeatherApp_InitBh1750(sensor_bus);
    if (bh1750_status == DEVICE_STATUS_OK)
    {
      WeatherApp_LogInitializedAddress("BH1750", bh1750.address_7bit);
    }
    else
    {
      WeatherApp_LogStatus("BH1750",
                           "initialization",
                           BH1750_StatusString(bh1750_status));
    }
  }

  if (APP_CONFIG_ENABLE_RAIN_SENSOR != 0U)
  {
    rain_status = WeatherApp_InitRainSensor(rain_adc);
    if (rain_status == DEVICE_STATUS_OK)
    {
      (void)AppLog_Write("Rain sensor ADC calibrated on PA0; digital input on PA1\r\n");
    }
    else
    {
      WeatherApp_LogStatus("Rain sensor",
                           "initialization",
                           RainSensor_StatusString(rain_status));
    }
  }

  last_sample_tick = HAL_GetTick() - APP_CONFIG_SAMPLE_PERIOD_MS;
}

void WeatherApp_Run(void)
{
  uint32_t current_tick = HAL_GetTick();

  if ((current_tick - last_sample_tick) < APP_CONFIG_SAMPLE_PERIOD_MS)
  {
    return;
  }
  last_sample_tick = current_tick;

  WeatherApp_ReadBme280();
  WeatherApp_ReadBh1750();
  WeatherApp_ReadRainSensor();
  WeatherApp_UpdateDisplay();

  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

const WeatherData *WeatherApp_GetLatestData(void)
{
  return &latest_weather_data;
}
