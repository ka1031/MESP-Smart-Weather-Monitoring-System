#ifndef BME280_H
#define BME280_H

#include "device_status.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_percent;
  float pressure_hpa;
} BME280_Measurement;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address_7bit;
  uint8_t initialized;
} BME280_Handle;

DeviceStatus BME280_Init(BME280_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit);
DeviceStatus BME280_Read(BME280_Handle *device,
                         BME280_Measurement *measurement);

#endif /* BME280_H */
