#ifndef SD_PROBE_H
#define SD_PROBE_H

#include "stm32f1xx_hal.h"

typedef enum {
  SD_PROBE_OK, SD_PROBE_ARGUMENT, SD_PROBE_SPI_INIT, SD_PROBE_SPI_IO,
  SD_PROBE_DEADLINE, SD_PROBE_READY_TIMEOUT, SD_PROBE_NO_RESPONSE,
  SD_PROBE_RESPONSE, SD_PROBE_ECHO, SD_PROBE_INIT_TIMEOUT,
  SD_PROBE_VOLTAGE, SD_PROBE_TOKEN_TIMEOUT, SD_PROBE_TOKEN,
  SD_PROBE_CRC, SD_PROBE_CAPACITY, SD_PROBE_WRITE_REJECT, SD_PROBE_WRITE_STATUS
} SDProbe_Status;

typedef struct {
  SDProbe_Status status;
  HAL_StatusTypeDef hal_status;
  uint32_t hal_error;
  uint32_t elapsed_ms;
  uint8_t command, response, token, last_rx;
  uint8_t block_addressed;
} SDProbe_Result;

/* Initializes SD v1/v2 in SPI mode and reads sector zero at /256 throughout.
 * No write/erase/format commands. Caller supplies a 512-byte output buffer.
 * GPIO/clocks/peripheral ownership remains with CubeMX. */
SDProbe_Result SDProbe_Run(SPI_HandleTypeDef *spi, GPIO_TypeDef *cs_port,
                           uint16_t cs_pin, uint8_t sector[512]);
const char *SDProbe_StatusName(SDProbe_Status status);

#endif
