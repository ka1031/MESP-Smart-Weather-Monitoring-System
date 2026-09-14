#include "rain_sensor.h"

#include <stddef.h>

static DeviceStatus RainSensor_FromHalStatus(HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return DEVICE_STATUS_OK;
  }

  if (status == HAL_TIMEOUT)
  {
    return DEVICE_STATUS_TIMEOUT;
  }

  return DEVICE_STATUS_IO_ERROR;
}

static uint8_t RainSensor_CalculatePercent(const RainSensor_Handle *sensor,
                                           uint16_t raw)
{
  uint32_t numerator;
  uint32_t denominator;

  if (sensor->config.dry_raw > sensor->config.wet_raw)
  {
    if (raw >= sensor->config.dry_raw)
    {
      return 0U;
    }
    if (raw <= sensor->config.wet_raw)
    {
      return 100U;
    }

    numerator = (uint32_t)sensor->config.dry_raw - raw;
    denominator = (uint32_t)sensor->config.dry_raw - sensor->config.wet_raw;
  }
  else
  {
    if (raw <= sensor->config.dry_raw)
    {
      return 0U;
    }
    if (raw >= sensor->config.wet_raw)
    {
      return 100U;
    }

    numerator = (uint32_t)raw - sensor->config.dry_raw;
    denominator = (uint32_t)sensor->config.wet_raw - sensor->config.dry_raw;
  }

  return (uint8_t)((numerator * 100U + (denominator / 2U)) / denominator);
}

DeviceStatus RainSensor_Init(RainSensor_Handle *sensor,
                             ADC_HandleTypeDef *adc,
                             const RainSensor_Config *config)
{
  HAL_StatusTypeDef hal_status;

  if ((sensor == NULL) || (adc == NULL) || (config == NULL) ||
      (config->digital_port == NULL) || (config->digital_pin == 0U) ||
      (config->sample_count == 0U) || (config->adc_timeout_ms == 0U) ||
      (config->dry_raw == config->wet_raw))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  sensor->adc = adc;
  sensor->config = *config;
  sensor->initialized = 0U;

  hal_status = HAL_ADCEx_Calibration_Start(adc);
  if (hal_status != HAL_OK)
  {
    return RainSensor_FromHalStatus(hal_status);
  }

  sensor->initialized = 1U;
  return DEVICE_STATUS_OK;
}

DeviceStatus RainSensor_Read(RainSensor_Handle *sensor,
                             RainSensor_Measurement *measurement)
{
  uint32_t sample_sum = 0U;
  uint8_t sample;
  GPIO_PinState digital_state;
  HAL_StatusTypeDef hal_status;

  if ((sensor == NULL) || (measurement == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (sensor->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  for (sample = 0U; sample < sensor->config.sample_count; ++sample)
  {
    hal_status = HAL_ADC_Start(sensor->adc);
    if (hal_status != HAL_OK)
    {
      return RainSensor_FromHalStatus(hal_status);
    }

    hal_status = HAL_ADC_PollForConversion(sensor->adc,
                                           sensor->config.adc_timeout_ms);
    if (hal_status != HAL_OK)
    {
      (void)HAL_ADC_Stop(sensor->adc);
      return RainSensor_FromHalStatus(hal_status);
    }

    sample_sum += HAL_ADC_GetValue(sensor->adc);
    hal_status = HAL_ADC_Stop(sensor->adc);
    if (hal_status != HAL_OK)
    {
      return RainSensor_FromHalStatus(hal_status);
    }
  }

  measurement->raw = (uint16_t)(sample_sum / sensor->config.sample_count);
  measurement->percent = RainSensor_CalculatePercent(sensor, measurement->raw);

  digital_state = HAL_GPIO_ReadPin(sensor->config.digital_port,
                                   sensor->config.digital_pin);
  if (sensor->config.digital_active_low != 0U)
  {
    measurement->wet = (digital_state == GPIO_PIN_RESET) ? 1U : 0U;
  }
  else
  {
    measurement->wet = (digital_state == GPIO_PIN_SET) ? 1U : 0U;
  }

  return DEVICE_STATUS_OK;
}

const char *RainSensor_StatusString(DeviceStatus status)
{
  switch (status)
  {
    case DEVICE_STATUS_OK:
      return "ok";
    case DEVICE_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case DEVICE_STATUS_NOT_INITIALIZED:
      return "not initialized";
    case DEVICE_STATUS_IO_ERROR:
      return "ADC error";
    case DEVICE_STATUS_TIMEOUT:
      return "ADC timeout";
    default:
      return "unknown error";
  }
}
