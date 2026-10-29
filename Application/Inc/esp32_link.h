#ifndef ESP32_LINK_H
#define ESP32_LINK_H

#include "stm32f1xx_hal.h"
#include "weather_data.h"
#include <stddef.h>

#define ESP32_LINK_PACKET_CAPACITY 320U

typedef struct {
  UART_HandleTypeDef *uart;
  uint32_t next_sequence, sent_packets, failed_packets;
  HAL_StatusTypeDef last_status;
  char packet[ESP32_LINK_PACKET_CAPACITY];
} ESP32Link_Handle;

/* Wire format: $MESP,1,sequence,<weather CSV fields>*CRC16\r\n.
 * Returns the wire length (excluding NUL), or zero without a usable packet.
 * CRC-16/CCITT-FALSE covers the bytes between '$' and '*', excluding both. */
size_t ESP32Link_Format(char *buffer, size_t capacity, const WeatherData *sample,
                        uint32_t sample_tick, uint32_t sequence);

/* Uses the CubeMX-generated USART2 handle. Does not configure pins or clocks. */
HAL_StatusTypeDef ESP32Link_Init(ESP32Link_Handle *link, UART_HandleTypeDef *uart);
/* One bounded polling transmission. No retry, ACK, or remote-delivery guarantee.
 * Call from the main loop only; samples are not queued. */
HAL_StatusTypeDef ESP32Link_Send(ESP32Link_Handle *link, const WeatherData *sample,
                                uint32_t sample_tick);

#endif
