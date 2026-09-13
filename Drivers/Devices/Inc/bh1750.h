#ifndef BH1750_H
#define BH1750_H

#include "device_status.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address_7bit;
  uint8_t initialized;
} BH1750_Handle;

DeviceStatus BH1750_Init(BH1750_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit);
DeviceStatus BH1750_ReadLux(BH1750_Handle *device, float *illuminance_lux);

#endif /* BH1750_H */
