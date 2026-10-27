#ifndef RTC_CONSOLE_H
#define RTC_CONSOLE_H

#include "ds3231.h"

/* USART1 only. Receive in interrupts; execute commands from the main loop. */
HAL_StatusTypeDef RTCConsole_Init(UART_HandleTypeDef *uart, DS3231_Handle *rtc);
void RTCConsole_Poll(void);
void RTCConsole_IRQHandler(void);

#endif /* RTC_CONSOLE_H */
