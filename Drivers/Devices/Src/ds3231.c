#include "ds3231.h"

#include <stddef.h>

#define DS3231_ADDRESS       (0x68U)
#define DS3231_CONTROL       (0x0EU)
#define DS3231_STATUS        (0x0FU)
#define DS3231_OSF           (0x80U)
#define DS3231_ALARM_FLAGS   (0x03U)
#define DS3231_TIMEOUT_MS    (100U)

static DeviceStatus FromHal(HAL_StatusTypeDef status)
{
  if (status == HAL_OK) return DEVICE_STATUS_OK;
  if (status == HAL_TIMEOUT) return DEVICE_STATUS_TIMEOUT;
  return DEVICE_STATUS_IO_ERROR;
}

static DeviceStatus ReadRegisters(DS3231_Handle *device, uint8_t reg,
                                   uint8_t *data, uint16_t size)
{
  return FromHal(HAL_I2C_Mem_Read(device->i2c,
      (uint16_t)((uint16_t)device->address_7bit << 1), reg,
      I2C_MEMADD_SIZE_8BIT, data, size, DS3231_TIMEOUT_MS));
}

static DeviceStatus WriteRegisters(DS3231_Handle *device, uint8_t reg,
                                    uint8_t *data, uint16_t size)
{
  return FromHal(HAL_I2C_Mem_Write(device->i2c,
      (uint16_t)((uint16_t)device->address_7bit << 1), reg,
      I2C_MEMADD_SIZE_8BIT, data, size, DS3231_TIMEOUT_MS));
}

static uint8_t EncodeBcd(uint8_t value)
{
  return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

static uint8_t DecodeBcd(uint8_t value, uint8_t *decoded)
{
  if (((value & 0x0FU) > 9U) || ((value >> 4) > 9U)) return 0U;
  *decoded = (uint8_t)((value >> 4) * 10U + (value & 0x0FU));
  return 1U;
}

uint8_t DS3231_IsValidDateTime(const DS3231_DateTime *date_time)
{
  static const uint8_t month_days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                      31U, 31U, 30U, 31U, 30U, 31U};
  uint8_t days;
  /* Restrict to 2000-2099, where the chip's leap-year handling applies. */
  if ((date_time == NULL) || (date_time->year < 2000U) ||
      (date_time->year > 2099U) || (date_time->month < 1U) ||
      (date_time->month > 12U) || (date_time->weekday < 1U) ||
      (date_time->weekday > 7U) || (date_time->hour > 23U) ||
      (date_time->minute > 59U) || (date_time->second > 59U)) return 0U;
  days = month_days[date_time->month - 1U];
  if ((date_time->month == 2U) && ((date_time->year % 4U) == 0U)) ++days;
  return (uint8_t)((date_time->day >= 1U) && (date_time->day <= days));
}

DeviceStatus DS3231_Init(DS3231_Handle *device, I2C_HandleTypeDef *i2c,
                         uint8_t address_7bit)
{
  HAL_StatusTypeDef ready;
  DeviceStatus status;
  uint8_t control;
  if (device == NULL) return DEVICE_STATUS_INVALID_ARGUMENT;
  device->initialized = 0U;
  if ((i2c == NULL) || (address_7bit != DS3231_ADDRESS))
    return DEVICE_STATUS_INVALID_ARGUMENT;
  device->i2c = i2c;
  device->address_7bit = address_7bit;
  ready = HAL_I2C_IsDeviceReady(i2c, (uint16_t)(address_7bit << 1),
                               2U, DS3231_TIMEOUT_MS);
  if (ready != HAL_OK)
    return (ready == HAL_TIMEOUT) ? DEVICE_STATUS_TIMEOUT : DEVICE_STATUS_NOT_FOUND;
  status = ReadRegisters(device, DS3231_CONTROL, &control, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  if ((control & 0x80U) != 0U)
  {
    control &= (uint8_t)~0x80U; /* Enable oscillator during future battery backup. */
    status = WriteRegisters(device, DS3231_CONTROL, &control, 1U);
    if (status != DEVICE_STATUS_OK) return status;
  }
  /* Never clear OSF or overwrite the date at startup. */
  device->initialized = 1U;
  return DEVICE_STATUS_OK;
}

DeviceStatus DS3231_ReadDateTime(DS3231_Handle *device,
                                 DS3231_DateTime *date_time)
{
  uint8_t raw[7];
  uint8_t flags;
  uint8_t year;
  DS3231_DateTime decoded = {0};
  DeviceStatus status;
  if ((device == NULL) || (date_time == NULL)) return DEVICE_STATUS_INVALID_ARGUMENT;
  if (device->initialized == 0U) return DEVICE_STATUS_NOT_INITIALIZED;
  status = ReadRegisters(device, DS3231_STATUS, &flags, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  if ((flags & DS3231_OSF) != 0U) return DEVICE_STATUS_TIME_NOT_SET;
  /* A single burst reads the chip's coherent snapshot, including midnight rollover. */
  status = ReadRegisters(device, 0U, raw, sizeof(raw));
  if (status != DEVICE_STATUS_OK) return status;
  status = ReadRegisters(device, DS3231_STATUS, &flags, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  if ((flags & DS3231_OSF) != 0U) return DEVICE_STATUS_TIME_NOT_SET;
  if (((raw[0] & 0x80U) != 0U) || ((raw[1] & 0x80U) != 0U) ||
      ((raw[2] & 0x80U) != 0U) || ((raw[3] & 0xF8U) != 0U) ||
      ((raw[4] & 0xC0U) != 0U) || ((raw[5] & 0xE0U) != 0U))
    return DEVICE_STATUS_DATA_ERROR; /* Includes unsupported century bit. */
  if (!DecodeBcd(raw[0], &decoded.second) ||
      !DecodeBcd(raw[1], &decoded.minute) ||
      !DecodeBcd(raw[4], &decoded.day) ||
      !DecodeBcd(raw[5], &decoded.month) || !DecodeBcd(raw[6], &year))
    return DEVICE_STATUS_DATA_ERROR;
  decoded.year = (uint16_t)(2000U + year);
  decoded.weekday = raw[3];
  if ((raw[2] & 0x40U) != 0U)
  {
    if (!DecodeBcd(raw[2] & 0x1FU, &decoded.hour) ||
        (decoded.hour < 1U) || (decoded.hour > 12U)) return DEVICE_STATUS_DATA_ERROR;
    decoded.hour = (uint8_t)((decoded.hour % 12U) +
                             (((raw[2] & 0x20U) != 0U) ? 12U : 0U));
  }
  else if (!DecodeBcd(raw[2], &decoded.hour)) return DEVICE_STATUS_DATA_ERROR;
  if (!DS3231_IsValidDateTime(&decoded)) return DEVICE_STATUS_DATA_ERROR;
  *date_time = decoded;
  return DEVICE_STATUS_OK;
}

DeviceStatus DS3231_SetDateTime(DS3231_Handle *device,
                                const DS3231_DateTime *date_time)
{
  uint8_t raw[7];
  uint8_t flags;
  DeviceStatus status;
  if ((device == NULL) || !DS3231_IsValidDateTime(date_time))
    return DEVICE_STATUS_INVALID_ARGUMENT;
  if (device->initialized == 0U) return DEVICE_STATUS_NOT_INITIALIZED;
  status = ReadRegisters(device, DS3231_STATUS, &flags, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  /* Mark time invalid before updating: a partial write must not look valid.
     Write ones to A1F/A2F so concurrently arriving alarms are not cleared. */
  flags |= DS3231_OSF | DS3231_ALARM_FLAGS;
  status = WriteRegisters(device, DS3231_STATUS, &flags, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  raw[0] = EncodeBcd(date_time->second);
  raw[1] = EncodeBcd(date_time->minute);
  raw[2] = EncodeBcd(date_time->hour); /* Always write 24-hour mode. */
  raw[3] = date_time->weekday;
  raw[4] = EncodeBcd(date_time->day);
  raw[5] = EncodeBcd(date_time->month);
  raw[6] = EncodeBcd((uint8_t)(date_time->year - 2000U));
  status = WriteRegisters(device, 0U, raw, sizeof(raw));
  if (status != DEVICE_STATUS_OK) return status;
  status = ReadRegisters(device, DS3231_STATUS, &flags, 1U);
  if (status != DEVICE_STATUS_OK) return status;
  flags = (uint8_t)((flags & (uint8_t)~DS3231_OSF) | DS3231_ALARM_FLAGS);
  return WriteRegisters(device, DS3231_STATUS, &flags, 1U);
}

const char *DS3231_StatusString(DeviceStatus status)
{
  switch (status)
  {
    case DEVICE_STATUS_OK: return "ok";
    case DEVICE_STATUS_TIME_NOT_SET: return "RTC time not set";
    case DEVICE_STATUS_INVALID_ARGUMENT: return "invalid date or argument";
    case DEVICE_STATUS_NOT_INITIALIZED: return "not initialized";
    case DEVICE_STATUS_NOT_FOUND: return "not found at 0x68";
    case DEVICE_STATUS_TIMEOUT: return "I2C timeout";
    case DEVICE_STATUS_DATA_ERROR: return "invalid RTC calendar";
    default: return "I2C error";
  }
}
