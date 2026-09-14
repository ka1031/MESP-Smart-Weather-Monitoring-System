#include "temporary_i2c_scanner.h"

#include "app_log.h"

#include <stddef.h>
#include <stdio.h>

#define TEMPORARY_I2C_SCANNER_FIRST_ADDRESS  (0x03U)
#define TEMPORARY_I2C_SCANNER_LAST_ADDRESS   (0x77U)
#define TEMPORARY_I2C_SCANNER_TRIALS         (1U)
#define TEMPORARY_I2C_SCANNER_TIMEOUT_MS     (5U)

uint8_t TemporaryI2CScanner_Run(I2C_HandleTypeDef *i2c)
{
  uint8_t address;
  uint8_t responding_devices = 0U;
  char message[64];

  (void)AppLog_Write(
      "TEMPORARY I2C1 scan: probing 7-bit addresses 0x03 through 0x77\r\n");

  if (i2c == NULL)
  {
    (void)AppLog_Write("TEMPORARY I2C1 scan failed: invalid I2C handle\r\n");
    return 0U;
  }

  for (address = TEMPORARY_I2C_SCANNER_FIRST_ADDRESS;
       address <= TEMPORARY_I2C_SCANNER_LAST_ADDRESS;
       ++address)
  {
    if (HAL_I2C_IsDeviceReady(i2c,
                              (uint16_t)((uint16_t)address << 1),
                              TEMPORARY_I2C_SCANNER_TRIALS,
                              TEMPORARY_I2C_SCANNER_TIMEOUT_MS) == HAL_OK)
    {
      ++responding_devices;
      (void)snprintf(message,
                     sizeof(message),
                     "I2C1 device found at 7-bit address 0x%02X\r\n",
                     address);
      (void)AppLog_Write(message);
    }
  }

  if (responding_devices == 0U)
  {
    (void)AppLog_Write(
        "TEMPORARY I2C1 scan complete: no responding addresses found\r\n");
  }
  else
  {
    (void)snprintf(message,
                   sizeof(message),
                   "TEMPORARY I2C1 scan complete: %u responding device(s)\r\n",
                   responding_devices);
    (void)AppLog_Write(message);
  }

  return responding_devices;
}
