#ifndef SD_SPI_H
#define SD_SPI_H
#include "sd_probe.h"

typedef struct {
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  uint8_t initialized, block_addressed;
  uint32_t sectors, operation_start;
  SDProbe_Result result;
} SD_SPI_Handle;

void SD_SPI_Bind(SD_SPI_Handle *card, SPI_HandleTypeDef *spi,
                 GPIO_TypeDef *port, uint16_t pin);
/* Bounds a complete filesystem operation, not just one sector transfer. */
void SD_SPI_BeginOperation(SD_SPI_Handle *card);
HAL_StatusTypeDef SD_SPI_Init(SD_SPI_Handle *card);
HAL_StatusTypeDef SD_SPI_Read(SD_SPI_Handle *card, uint32_t sector,
                              uint8_t *data, uint32_t count);
HAL_StatusTypeDef SD_SPI_Write(SD_SPI_Handle *card, uint32_t sector,
                               const uint8_t *data, uint32_t count);
HAL_StatusTypeDef SD_SPI_Sync(SD_SPI_Handle *card);
#endif
