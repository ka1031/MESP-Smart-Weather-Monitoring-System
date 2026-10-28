#ifndef SD_DIAGNOSTIC_H
#define SD_DIAGNOSTIC_H
#include "stm32f1xx_hal.h"
#include <stddef.h>

void SDDiagnostic_Init(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin);
void SDDiagnostic_Request(void);
void SDDiagnostic_Process(void);
void SDDiagnostic_Report(void);
void SDDiagnostic_Display(char *buffer, size_t capacity);
#endif
