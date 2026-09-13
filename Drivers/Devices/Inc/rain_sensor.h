#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include "device_status.h"

#include <stdint.h>

typedef struct
{
  uint16_t raw;
  uint8_t percent;
  uint8_t wet;
} RainSensor_Measurement;

/* ADC acquisition remains in the application until ADC is enabled in CubeMX. */
DeviceStatus RainSensor_ProcessRaw(uint16_t raw,
                                   RainSensor_Measurement *measurement);

#endif /* RAIN_SENSOR_H */
