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
  uint16_t dig_t1;
  int16_t dig_t2;
  int16_t dig_t3;
  uint16_t dig_p1;
  int16_t dig_p2;
  int16_t dig_p3;
  int16_t dig_p4;
  int16_t dig_p5;
  int16_t dig_p6;
  int16_t dig_p7;
  int16_t dig_p8;
  int16_t dig_p9;
  uint8_t dig_h1;
  int16_t dig_h2;
  uint8_t dig_h3;
  int16_t dig_h4;
  int16_t dig_h5;
  int8_t dig_h6;
} BME280_Calibration;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address_7bit;
  uint8_t initialized;
  BME280_Calibration calibration;
} BME280_Handle;

DeviceStatus BME280_Init(BME280_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit);
DeviceStatus BME280_Read(BME280_Handle *device,
                         BME280_Measurement *measurement);
const char *BME280_StatusString(DeviceStatus status);

#endif /* BME280_H */
