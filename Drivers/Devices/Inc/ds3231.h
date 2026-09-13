#ifndef DS3231_H
#define DS3231_H

#include "device_status.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} DS3231_DateTime;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address_7bit;
  uint8_t initialized;
} DS3231_Handle;

DeviceStatus DS3231_Init(DS3231_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit);
DeviceStatus DS3231_ReadDateTime(DS3231_Handle *device,
                                 DS3231_DateTime *date_time);
DeviceStatus DS3231_SetDateTime(DS3231_Handle *device,
                                const DS3231_DateTime *date_time);

#endif /* DS3231_H */
