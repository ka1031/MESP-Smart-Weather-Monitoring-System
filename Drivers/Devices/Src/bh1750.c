#include "bh1750.h"

#include <stddef.h>

#define BH1750_ADDRESS_LOW                 (0x23U)
#define BH1750_ADDRESS_HIGH                (0x5CU)

#define BH1750_COMMAND_POWER_DOWN          (0x00U)
#define BH1750_COMMAND_POWER_ON            (0x01U)
#define BH1750_COMMAND_RESET               (0x07U)
#define BH1750_COMMAND_ONE_TIME_HIGH_RES   (0x20U)

#define BH1750_I2C_TIMEOUT_MS              (100U)
#define BH1750_MEASUREMENT_TIME_MS         (180U)
#define BH1750_READY_TRIALS                 (2U)
#define BH1750_RESULT_LENGTH                (2U)

static DeviceStatus BH1750_FromHalStatus(HAL_StatusTypeDef status)
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

static DeviceStatus BH1750_SendCommand(BH1750_Handle *device, uint8_t command)
{
  HAL_StatusTypeDef hal_status;
  uint16_t hal_address = (uint16_t)((uint16_t)device->address_7bit << 1);

  /* Each opcode is a separate I2C transaction as required by the device. */
  hal_status = HAL_I2C_Master_Transmit(device->i2c,
                                      hal_address,
                                      &command,
                                      1U,
                                      BH1750_I2C_TIMEOUT_MS);
  return BH1750_FromHalStatus(hal_status);
}

DeviceStatus BH1750_Init(BH1750_Handle *device,
                         I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit)
{
  HAL_StatusTypeDef hal_status;
  DeviceStatus status;

  if ((device == NULL) || (i2c == NULL) ||
      ((address_7bit != BH1750_ADDRESS_LOW) &&
       (address_7bit != BH1750_ADDRESS_HIGH)))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  device->i2c = i2c;
  device->address_7bit = address_7bit;
  device->initialized = 0U;

  hal_status = HAL_I2C_IsDeviceReady(i2c,
                                    (uint16_t)((uint16_t)address_7bit << 1),
                                    BH1750_READY_TRIALS,
                                    BH1750_I2C_TIMEOUT_MS);
  if (hal_status != HAL_OK)
  {
    return (hal_status == HAL_TIMEOUT) ? DEVICE_STATUS_TIMEOUT
                                       : DEVICE_STATUS_NOT_FOUND;
  }

  status = BH1750_SendCommand(device, BH1750_COMMAND_POWER_ON);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BH1750_SendCommand(device, BH1750_COMMAND_RESET);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BH1750_SendCommand(device, BH1750_COMMAND_POWER_DOWN);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  device->initialized = 1U;
  return DEVICE_STATUS_OK;
}

DeviceStatus BH1750_ReadLux(BH1750_Handle *device, float *illuminance_lux)
{
  uint8_t raw_data[BH1750_RESULT_LENGTH];
  uint16_t raw_light;
  uint16_t hal_address;
  HAL_StatusTypeDef hal_status;
  DeviceStatus status;

  if ((device == NULL) || (illuminance_lux == NULL))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  if (device->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  status = BH1750_SendCommand(device, BH1750_COMMAND_POWER_ON);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  status = BH1750_SendCommand(device, BH1750_COMMAND_ONE_TIME_HIGH_RES);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  /* High-resolution conversion is 120 ms typical and 180 ms maximum. */
  HAL_Delay(BH1750_MEASUREMENT_TIME_MS);

  hal_address = (uint16_t)((uint16_t)device->address_7bit << 1);
  hal_status = HAL_I2C_Master_Receive(device->i2c,
                                     hal_address,
                                     raw_data,
                                     sizeof(raw_data),
                                     BH1750_I2C_TIMEOUT_MS);
  status = BH1750_FromHalStatus(hal_status);
  if (status != DEVICE_STATUS_OK)
  {
    return status;
  }

  raw_light = (uint16_t)(((uint16_t)raw_data[0] << 8) | raw_data[1]);
  *illuminance_lux = (float)raw_light / 1.2F;
  return DEVICE_STATUS_OK;
}

const char *BH1750_StatusString(DeviceStatus status)
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
    default:
      return "unknown error";
  }
}
