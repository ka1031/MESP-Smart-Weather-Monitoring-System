#include "rain_sensor.h"

#include <stddef.h>

DeviceStatus RainSensor_ProcessRaw(uint16_t raw,
                                   RainSensor_Measurement *measurement)
{
  (void)raw;

  if (measurement == NULL)
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  return DEVICE_STATUS_NOT_IMPLEMENTED;
}
