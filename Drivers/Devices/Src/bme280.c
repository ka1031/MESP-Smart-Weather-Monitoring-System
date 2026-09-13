#include "bme280.h"

#include <stddef.h>

DeviceStatus BME280_Init(BME280_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit)
{
  if ((device == NULL) || (i2c == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  device->i2c = i2c;
  device->address_7bit = address_7bit;
  device->initialized = 0U;
  return DEVICE_STATUS_NOT_IMPLEMENTED;
}

DeviceStatus BME280_Read(BME280_Handle *device,
                         BME280_Measurement *measurement)
{
  if ((device == NULL) || (measurement == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  return DEVICE_STATUS_NOT_IMPLEMENTED;
}
