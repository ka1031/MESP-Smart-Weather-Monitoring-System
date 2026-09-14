#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include "device_status.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *digital_port;
  uint16_t digital_pin;
  uint16_t dry_raw;
  uint16_t wet_raw;
  uint32_t adc_timeout_ms;
  uint8_t digital_active_low;
  uint8_t sample_count;
} RainSensor_Config;

typedef struct
{
  ADC_HandleTypeDef *adc;
  RainSensor_Config config;
  uint8_t initialized;
} RainSensor_Handle;

typedef struct
{
  uint16_t raw;
  uint8_t percent;
  uint8_t wet;
} RainSensor_Measurement;

DeviceStatus RainSensor_Init(RainSensor_Handle *sensor,
                             ADC_HandleTypeDef *adc,
                             const RainSensor_Config *config);
DeviceStatus RainSensor_Read(RainSensor_Handle *sensor,
                             RainSensor_Measurement *measurement);
const char *RainSensor_StatusString(DeviceStatus status);

#endif /* RAIN_SENSOR_H */
