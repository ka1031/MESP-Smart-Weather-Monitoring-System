#include "esp32_link.h"
#include "app_config.h"
#include "weather_csv.h"
#include <stdio.h>

static uint16_t PacketCrc(const char *text, size_t length)
{
  uint16_t crc = 0xFFFFU;
  for (size_t i = 0U; i < length; ++i) {
    crc ^= (uint16_t)(uint8_t)text[i] << 8;
    for (unsigned bit = 0U; bit < 8U; ++bit)
      crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
  }
  return crc;
}

size_t ESP32Link_Format(char *buffer, size_t capacity, const WeatherData *sample,
                        uint32_t sample_tick, uint32_t sequence)
{
  if (buffer == NULL || capacity == 0U) return 0U;
  buffer[0] = '\0';
  if (sample == NULL) return 0U;
  int prefix = snprintf(buffer, capacity, "$MESP,2,%lu,", (unsigned long)sequence);
  if (prefix < 0 || (size_t)prefix >= capacity) goto invalid;
  size_t used = (size_t)prefix;
  /* Seven suffix bytes replace two CSV terminator bytes: reserve five more. */
  if (capacity - used <= 5U) goto invalid;
  size_t csv_length = WeatherCSV_Format(buffer + used, capacity - used - 5U,
                                        sample, sample_tick);
  if (csv_length < 2U) goto invalid;
  used += csv_length - 2U; /* Replace the CSV CRLF with *CRC16 + CRLF. */
  uint16_t crc = PacketCrc(buffer + 1U, used - 1U);
  int suffix = snprintf(buffer + used, capacity - used, "*%04X\r\n", (unsigned)crc);
  if (suffix != 7 || (size_t)suffix >= capacity - used) goto invalid;
  return used + (size_t)suffix;
invalid:
  buffer[0] = '\0';
  return 0U;
}

HAL_StatusTypeDef ESP32Link_Init(ESP32Link_Handle *link, UART_HandleTypeDef *uart)
{
  if (link == NULL) return HAL_ERROR;
  *link = (ESP32Link_Handle){.last_status = HAL_ERROR};
  if (uart == NULL || uart->Instance != USART2) return HAL_ERROR;
  link->uart = uart;
  link->last_status = HAL_OK;
  return HAL_OK;
}

HAL_StatusTypeDef ESP32Link_Send(ESP32Link_Handle *link, const WeatherData *sample,
                                uint32_t sample_tick)
{
  if (link == NULL) return HAL_ERROR;
  uint32_t sequence = link->next_sequence++;
  link->last_status = HAL_ERROR;
  if (link->uart != NULL && sample != NULL) {
    size_t length = ESP32Link_Format(link->packet, sizeof(link->packet), sample,
                                     sample_tick, sequence);
    if (length != 0U && length <= UINT16_MAX)
      link->last_status = HAL_UART_Transmit(link->uart, (const uint8_t *)link->packet,
          (uint16_t)length, APP_CONFIG_ESP32_UART_TIMEOUT_MS);
  }
  if (link->last_status == HAL_OK) ++link->sent_packets;
  else ++link->failed_packets;
  return link->last_status;
}
