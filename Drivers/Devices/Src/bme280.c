#include "bme280.h"

#include <stddef.h>

#define BME280_ADDRESS_LOW             (0x76U)
#define BME280_ADDRESS_HIGH            (0x77U)
#define BME280_CHIP_ID                 (0x60U)

#define BME280_REG_CALIB_T_P_START     (0x88U)
#define BME280_REG_CHIP_ID             (0xD0U)
#define BME280_REG_RESET               (0xE0U)
#define BME280_REG_CALIB_H_START       (0xE1U)
#define BME280_REG_CTRL_HUM            (0xF2U)
#define BME280_REG_STATUS              (0xF3U)
#define BME280_REG_CTRL_MEAS           (0xF4U)
#define BME280_REG_CONFIG              (0xF5U)
#define BME280_REG_DATA_START          (0xF7U)

#define BME280_RESET_COMMAND           (0xB6U)
#define BME280_STATUS_MEASURING        (1U << 3)
#define BME280_STATUS_IMAGE_UPDATE     (1U << 0)

#define BME280_OVERSAMPLING_X1         (0x01U)
#define BME280_MODE_SLEEP              (0x00U)
#define BME280_MODE_FORCED             (0x01U)
#define BME280_FILTER_OFF              (0x00U)

#define BME280_CTRL_HUM_X1             (BME280_OVERSAMPLING_X1)
#define BME280_CTRL_MEAS_SLEEP         ((BME280_OVERSAMPLING_X1 << 5) | \
                                        (BME280_OVERSAMPLING_X1 << 2) | \
                                        BME280_MODE_SLEEP)
#define BME280_CTRL_MEAS_FORCED        ((BME280_OVERSAMPLING_X1 << 5) | \
                                        (BME280_OVERSAMPLING_X1 << 2) | \
                                        BME280_MODE_FORCED)
#define BME280_CONFIG_FILTER_OFF       (BME280_FILTER_OFF << 2)

#define BME280_CALIB_T_P_LENGTH        (26U)
#define BME280_CALIB_H_LENGTH          (7U)
#define BME280_DATA_LENGTH             (8U)
#define BME280_I2C_TIMEOUT_MS          (100U)
#define BME280_RESET_DELAY_MS          (2U)
#define BME280_NVM_TIMEOUT_MS          (20U)
#define BME280_MEASUREMENT_DELAY_MS    (10U)
#define BME280_MEASUREMENT_TIMEOUT_MS  (20U)

#define BME280_RAW_20BIT_DISABLED      (0x80000L)
#define BME280_RAW_HUMIDITY_DISABLED   (0x8000L)

static uint16_t BME280_ReadU16LittleEndian(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static int16_t BME280_ReadS16LittleEndian(const uint8_t *data)
{
  return (int16_t)BME280_ReadU16LittleEndian(data);
}

static int16_t BME280_SignExtend12(uint16_t value)
{
  value &= 0x0FFFU;
  if ((value & 0x0800U) != 0U)
  {
    value |= 0xF000U;
  }

  return (int16_t)value;
}

static DeviceStatus BME280_FromHalStatus(HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return DEVICE_STATUS_OK;
  }

  if (status == HAL_TIMEOUT)
  {
    return DEVICE_STATUS_TIMEOUT;
  }

  return DEVICE_STATUS_IO_ERROR;
}

static DeviceStatus BME280_ReadRegisters(BME280_Handle *device,
                                         uint8_t start_register,
                                         uint8_t *data,
                                         uint16_t length)
{
  HAL_StatusTypeDef hal_status;
  uint16_t hal_address = (uint16_t)((uint16_t)device->address_7bit << 1);

  hal_status = HAL_I2C_Mem_Read(device->i2c,
                               hal_address,
                               start_register,
                               I2C_MEMADD_SIZE_8BIT,
                               data,
                               length,
                               BME280_I2C_TIMEOUT_MS);
  return BME280_FromHalStatus(hal_status);
}

static DeviceStatus BME280_WriteRegister(BME280_Handle *device,
                                         uint8_t target_register,
                                         uint8_t value)
{
  HAL_StatusTypeDef hal_status;
  uint16_t hal_address = (uint16_t)((uint16_t)device->address_7bit << 1);

  hal_status = HAL_I2C_Mem_Write(device->i2c,
                                hal_address,
                                target_register,
                                I2C_MEMADD_SIZE_8BIT,
                                &value,
                                1U,
                                BME280_I2C_TIMEOUT_MS);
  return BME280_FromHalStatus(hal_status);
}

static DeviceStatus BME280_WaitForStatusClear(BME280_Handle *device,
                                               uint8_t status_mask,
                                               uint32_t timeout_ms)
{
  uint32_t start_tick = HAL_GetTick();
  uint8_t status_register;
  DeviceStatus status;

  do
  {
    status = BME280_ReadRegisters(device,
                                  BME280_REG_STATUS,
                                  &status_register,
                                  1U);
    if (status != DEVICE_STATUS_OK)
    {
      return status;
    }

    if ((status_register & status_mask) == 0U)
    {
      return DEVICE_STATUS_OK;
    }

    HAL_Delay(1U);
  } while ((HAL_GetTick() - start_tick) < timeout_ms);

  return DEVICE_STATUS_TIMEOUT;
}

static DeviceStatus BME280_ReadCalibration(BME280_Handle *device)
{
  uint8_t calibration_tp[BME280_CALIB_T_P_LENGTH];
  uint8_t calibration_h[BME280_CALIB_H_LENGTH];
  BME280_Calibration *calibration = &device->calibration;
  DeviceStatus status;

  status = BME280_ReadRegisters(device,
                                BME280_REG_CALIB_T_P_START,
                                calibration_tp,
                                sizeof(calibration_tp));
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_ReadRegisters(device,
                                BME280_REG_CALIB_H_START,
                                calibration_h,
                                sizeof(calibration_h));
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  calibration->dig_t1 = BME280_ReadU16LittleEndian(&calibration_tp[0]);
  calibration->dig_t2 = BME280_ReadS16LittleEndian(&calibration_tp[2]);
  calibration->dig_t3 = BME280_ReadS16LittleEndian(&calibration_tp[4]);
  calibration->dig_p1 = BME280_ReadU16LittleEndian(&calibration_tp[6]);
  calibration->dig_p2 = BME280_ReadS16LittleEndian(&calibration_tp[8]);
  calibration->dig_p3 = BME280_ReadS16LittleEndian(&calibration_tp[10]);
  calibration->dig_p4 = BME280_ReadS16LittleEndian(&calibration_tp[12]);
  calibration->dig_p5 = BME280_ReadS16LittleEndian(&calibration_tp[14]);
  calibration->dig_p6 = BME280_ReadS16LittleEndian(&calibration_tp[16]);
  calibration->dig_p7 = BME280_ReadS16LittleEndian(&calibration_tp[18]);
  calibration->dig_p8 = BME280_ReadS16LittleEndian(&calibration_tp[20]);
  calibration->dig_p9 = BME280_ReadS16LittleEndian(&calibration_tp[22]);
  calibration->dig_h1 = calibration_tp[25];
  calibration->dig_h2 = BME280_ReadS16LittleEndian(&calibration_h[0]);
  calibration->dig_h3 = calibration_h[2];
  calibration->dig_h4 = BME280_SignExtend12(
      (uint16_t)(((uint16_t)calibration_h[3] << 4) |
                 ((uint16_t)calibration_h[4] & 0x0FU)));
  calibration->dig_h5 = BME280_SignExtend12(
      (uint16_t)(((uint16_t)calibration_h[5] << 4) |
                 ((uint16_t)calibration_h[4] >> 4)));
  calibration->dig_h6 = (int8_t)calibration_h[6];

  if ((calibration->dig_t1 == 0U) ||
      (calibration->dig_t1 == UINT16_MAX) ||
      (calibration->dig_p1 == 0U) ||
      (calibration->dig_p1 == UINT16_MAX))
  {
    return DEVICE_STATUS_CALIBRATION_ERROR;
  }

  return DEVICE_STATUS_OK;
}

static int32_t BME280_CompensateTemperature(const BME280_Calibration *calibration,
                                             int32_t raw_temperature,
                                             int32_t *temperature_fine)
{
  /* Bosch datasheet integer compensation: result is degrees Celsius x 100. */
  int32_t var1;
  int32_t var2;

  var1 = ((((raw_temperature >> 3) -
            ((int32_t)calibration->dig_t1 << 1))) *
          (int32_t)calibration->dig_t2) >> 11;
  var2 = (((((raw_temperature >> 4) - (int32_t)calibration->dig_t1) *
             ((raw_temperature >> 4) - (int32_t)calibration->dig_t1)) >> 12) *
           (int32_t)calibration->dig_t3) >> 14;

  *temperature_fine = var1 + var2;
  return ((*temperature_fine * 5) + 128) >> 8;
}

static DeviceStatus BME280_CompensatePressure(
    const BME280_Calibration *calibration,
    int32_t raw_pressure,
    int32_t temperature_fine,
    uint32_t *pressure_q24_8)
{
  /* Bosch datasheet 64-bit compensation: result is pascals in Q24.8. */
  int64_t var1;
  int64_t var2;
  int64_t pressure;

  var1 = (int64_t)temperature_fine - 128000;
  var2 = var1 * var1 * (int64_t)calibration->dig_p6;
  var2 += (var1 * (int64_t)calibration->dig_p5) * 131072;
  var2 += (int64_t)calibration->dig_p4 * (((int64_t)1) << 35);
  var1 = ((var1 * var1 * (int64_t)calibration->dig_p3) >> 8) +
         ((var1 * (int64_t)calibration->dig_p2) * 4096);
  var1 = (((((int64_t)1) << 47) + var1) *
          (int64_t)calibration->dig_p1) >> 33;

  if (var1 == 0)
  {
    return DEVICE_STATUS_CALIBRATION_ERROR;
  }

  pressure = 1048576 - (int64_t)raw_pressure;
  pressure = (((pressure << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)calibration->dig_p9 *
          (pressure >> 13) * (pressure >> 13)) >> 25;
  var2 = ((int64_t)calibration->dig_p8 * pressure) >> 19;
  pressure = ((pressure + var1 + var2) >> 8) +
             ((int64_t)calibration->dig_p7 * 16);

  if ((pressure < 0) || (pressure > (int64_t)UINT32_MAX))
  {
    return DEVICE_STATUS_DATA_ERROR;
  }

  *pressure_q24_8 = (uint32_t)pressure;
  return DEVICE_STATUS_OK;
}

static uint32_t BME280_CompensateHumidity(
    const BME280_Calibration *calibration,
    int32_t raw_humidity,
    int32_t temperature_fine)
{
  /* Bosch datasheet integer compensation: result is %RH in Q22.10. */
  int32_t humidity;

  humidity = temperature_fine - 76800;
  humidity = (((((raw_humidity << 14) -
                  ((int32_t)calibration->dig_h4 * 1048576) -
                  ((int32_t)calibration->dig_h5 * humidity)) + 16384) >> 15) *
              (((((((humidity * (int32_t)calibration->dig_h6) >> 10) *
                    (((humidity * (int32_t)calibration->dig_h3) >> 11) +
                     32768)) >> 10) + 2097152) *
                 (int32_t)calibration->dig_h2 + 8192) >> 14));
  humidity -= (((((humidity >> 15) * (humidity >> 15)) >> 7) *
                (int32_t)calibration->dig_h1) >> 4);

  if (humidity < 0)
  {
    humidity = 0;
  }
  else if (humidity > 419430400)
  {
    humidity = 419430400;
  }

  return (uint32_t)(humidity >> 12);
}

DeviceStatus BME280_Init(BME280_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit)
{
  HAL_StatusTypeDef hal_status;
  DeviceStatus status;
  uint8_t chip_id;

  if ((device == NULL) || (i2c == NULL) ||
      ((address_7bit != BME280_ADDRESS_LOW) &&
       (address_7bit != BME280_ADDRESS_HIGH)))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  device->i2c = i2c;
  device->address_7bit = address_7bit;
  device->initialized = 0U;

  hal_status = HAL_I2C_IsDeviceReady(i2c,
                                    (uint16_t)((uint16_t)address_7bit << 1),
                                    2U,
                                    BME280_I2C_TIMEOUT_MS);
  if (hal_status != HAL_OK)
  {
    return (hal_status == HAL_TIMEOUT) ? DEVICE_STATUS_TIMEOUT
                                       : DEVICE_STATUS_NOT_FOUND;
  }

  status = BME280_ReadRegisters(device, BME280_REG_CHIP_ID, &chip_id, 1U);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  if (chip_id != BME280_CHIP_ID)
  {
    return DEVICE_STATUS_ID_MISMATCH;
  }

  status = BME280_WriteRegister(device,
                                BME280_REG_RESET,
                                BME280_RESET_COMMAND);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  HAL_Delay(BME280_RESET_DELAY_MS);
  status = BME280_WaitForStatusClear(device,
                                     BME280_STATUS_IMAGE_UPDATE,
                                     BME280_NVM_TIMEOUT_MS);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_ReadCalibration(device);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_WriteRegister(device,
                                BME280_REG_CONFIG,
                                BME280_CONFIG_FILTER_OFF);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_WriteRegister(device,
                                BME280_REG_CTRL_HUM,
                                BME280_CTRL_HUM_X1);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_WriteRegister(device,
                                BME280_REG_CTRL_MEAS,
                                BME280_CTRL_MEAS_SLEEP);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  device->initialized = 1U;
  return DEVICE_STATUS_OK;
}

DeviceStatus BME280_Read(BME280_Handle *device,
                         BME280_Measurement *measurement)
{
  uint8_t raw_data[BME280_DATA_LENGTH];
  int32_t raw_pressure;
  int32_t raw_temperature;
  int32_t raw_humidity;
  int32_t temperature_fine;
  int32_t temperature_centi_c;
  uint32_t pressure_q24_8;
  uint32_t humidity_q22_10;
  DeviceStatus status;

  if ((device == NULL) || (measurement == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  status = BME280_WriteRegister(device,
                                BME280_REG_CTRL_MEAS,
                                BME280_CTRL_MEAS_FORCED);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  HAL_Delay(BME280_MEASUREMENT_DELAY_MS);
  status = BME280_WaitForStatusClear(device,
                                     BME280_STATUS_MEASURING,
                                     BME280_MEASUREMENT_TIMEOUT_MS);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BME280_ReadRegisters(device,
                                BME280_REG_DATA_START,
                                raw_data,
                                sizeof(raw_data));
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  raw_pressure = ((int32_t)raw_data[0] << 12) |
                 ((int32_t)raw_data[1] << 4) |
                 ((int32_t)raw_data[2] >> 4);
  raw_temperature = ((int32_t)raw_data[3] << 12) |
                    ((int32_t)raw_data[4] << 4) |
                    ((int32_t)raw_data[5] >> 4);
  raw_humidity = ((int32_t)raw_data[6] << 8) | (int32_t)raw_data[7];

  if ((raw_pressure == BME280_RAW_20BIT_DISABLED) ||
      (raw_temperature == BME280_RAW_20BIT_DISABLED) ||
      (raw_humidity == BME280_RAW_HUMIDITY_DISABLED))
  {
    return DEVICE_STATUS_DATA_ERROR;
  }

  temperature_centi_c = BME280_CompensateTemperature(&device->calibration,
                                                       raw_temperature,
                                                       &temperature_fine);
  status = BME280_CompensatePressure(&device->calibration,
                                     raw_pressure,
                                     temperature_fine,
                                     &pressure_q24_8);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  humidity_q22_10 = BME280_CompensateHumidity(&device->calibration,
                                               raw_humidity,
                                               temperature_fine);

  measurement->temperature_c = (float)temperature_centi_c / 100.0F;
  measurement->pressure_hpa = (float)pressure_q24_8 / 25600.0F;
  measurement->humidity_percent = (float)humidity_q22_10 / 1024.0F;
  return DEVICE_STATUS_OK;
}

const char *BME280_StatusString(DeviceStatus status)
{
  switch (status)
  {
    case DEVICE_STATUS_OK:
      return "ok";
    case DEVICE_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case DEVICE_STATUS_NOT_INITIALIZED:
      return "not initialized";
    case DEVICE_STATUS_NOT_IMPLEMENTED:
      return "not implemented";
    case DEVICE_STATUS_NOT_FOUND:
      return "device not found";
    case DEVICE_STATUS_IO_ERROR:
      return "I2C error";
    case DEVICE_STATUS_DATA_ERROR:
      return "invalid measurement data";
    case DEVICE_STATUS_TIMEOUT:
      return "timeout";
    case DEVICE_STATUS_ID_MISMATCH:
      return "chip ID is not BME280";
    case DEVICE_STATUS_CALIBRATION_ERROR:
      return "invalid calibration data";
    default:
      return "unknown error";
  }
}
