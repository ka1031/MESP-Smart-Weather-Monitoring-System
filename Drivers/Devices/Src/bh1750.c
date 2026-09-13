#include "bh1750.h"

#include <stddef.h>

DeviceStatus BH1750_Init(BH1750_Handle *device,
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

DeviceStatus BH1750_ReadLux(BH1750_Handle *device, float *illuminance_lux)
{
  if ((device == NULL) || (illuminance_lux == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  return DEVICE_STATUS_NOT_IMPLEMENTED;
}
