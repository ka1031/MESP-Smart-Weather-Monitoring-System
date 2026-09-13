#include "ds3231.h"

#include <stddef.h>

DeviceStatus DS3231_Init(DS3231_Handle *device,
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

DeviceStatus DS3231_ReadDateTime(DS3231_Handle *device,
                                 DS3231_DateTime *date_time)
{
  if ((device == NULL) || (date_time == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  return DEVICE_STATUS_NOT_IMPLEMENTED;
}

DeviceStatus DS3231_SetDateTime(DS3231_Handle *device,
                                const DS3231_DateTime *date_time)
{
  if ((device == NULL) || (date_time == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  return DEVICE_STATUS_NOT_IMPLEMENTED;
}
