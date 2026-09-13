#ifndef APP_LOG_H
#define APP_LOG_H

#include "stm32f1xx_hal.h"

void AppLog_Init(UART_HandleTypeDef *uart);
HAL_StatusTypeDef AppLog_Write(const char *message);

#endif /* APP_LOG_H */
