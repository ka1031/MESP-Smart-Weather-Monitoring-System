#ifndef SD_DISKIO_H
#define SD_DISKIO_H
#include "sd_spi.h"
#include <stddef.h>
void SDDisk_DiagnosticText(char *buffer, size_t capacity);
void SDDisk_Report(void);
void SDDisk_Bind(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin);
void SDDisk_BeginMount(void);
void SDDisk_BeginOperation(void);
#endif
