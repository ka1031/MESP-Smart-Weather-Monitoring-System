#include "esp32_link.h"
#include "app_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef uart = {USART2, 0U};
static HAL_StatusTypeDef transmit_status;
static unsigned transmits;
static char transmitted[ESP32_LINK_PACKET_CAPACITY];

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *data,
                                    uint16_t size, uint32_t timeout)
{
  assert(h == &uart && size > 0U && size < sizeof(transmitted));
  assert(timeout == 50U);
  memcpy(transmitted, data, size); transmitted[size] = '\0';
  assert(data[size - 2U] == '\r' && data[size - 1U] == '\n');
  assert(memchr(data, '\0', size) == NULL);
  ++transmits;
  return transmit_status;
}

int main(void)
{
  WeatherData sample = {
    .temperature_c = -0.25F, .humidity_percent = 62.5F, .pressure_hpa = 1008.25F,
    .illuminance_lux = 123.75F, .rain_raw = 3000U, .rain_percent = 27U, .rain_wet = 0U,
    .timestamp = {2026U, 10U, 28U, 14U, 32U, 5U}, .valid_flags = 15U
  };
  const char expected[] = "$MESP,2,42,1234,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15,UNKNOWN,0*87F2\r\n";
  char packet[ESP32_LINK_PACKET_CAPACITY];
  size_t length = ESP32Link_Format(packet, sizeof(packet), &sample, 1234U, 42U);
  assert(length == strlen(expected) && strcmp(packet, expected) == 0);
  fputs(packet, stdout);

  /* Exact-size buffers succeed; all shorter buffers fail without a partial frame
     or writes beyond capacity. The expected CRC is an independent golden vector. */
  unsigned char guarded[ESP32_LINK_PACKET_CAPACITY + 2U];
  for (size_t capacity = 0U; capacity <= length + 2U; ++capacity) {
    memset(guarded, 0xA5, sizeof(guarded));
    size_t got = ESP32Link_Format((char *)guarded + 1U, capacity, &sample, 1234U, 42U);
    assert(guarded[0] == 0xA5U);
    for (size_t i = capacity + 1U; i < sizeof(guarded); ++i) assert(guarded[i] == 0xA5U);
    if (capacity > length) assert(got == length);
    else { assert(got == 0U); if (capacity) assert(guarded[1] == 0U); }
  }
  assert(ESP32Link_Format(NULL, sizeof(packet), &sample, 0U, 0U) == 0U);
  assert(ESP32Link_Format(packet, sizeof(packet), NULL, 0U, 0U) == 0U && packet[0] == '\0');

  WeatherData missing = sample;
  missing.valid_flags = 0U;
  assert(ESP32Link_Format(packet, sizeof(packet), &missing, UINT32_MAX, UINT32_MAX) > 0U);
  assert(strstr(packet, "4294967295,4294967295,,0,,,,,,,,0,UNKNOWN,0*") != NULL);
  fputs(packet, stdout);
  WeatherData extreme = sample;
  extreme.temperature_c = -1000000.0F;
  extreme.humidity_percent = extreme.pressure_hpa = extreme.illuminance_lux = 1000000.0F;
  extreme.rain_raw = UINT16_MAX; extreme.rain_percent = UINT8_MAX; extreme.rain_wet = 1U;
  assert(ESP32Link_Format(packet, sizeof(packet), &extreme, UINT32_MAX, UINT32_MAX) < sizeof(packet));
  assert(strstr(packet, "-1000000.00,1000000.00,1000000.00,1000000.00") != NULL);
  fputs(packet, stdout);
  extreme.temperature_c = NAN; extreme.humidity_percent = INFINITY;
  extreme.pressure_hpa = -INFINITY; extreme.illuminance_lux = 2000000.0F;
  assert(ESP32Link_Format(packet, sizeof(packet), &extreme, 0U, 0U) > 0U);
  assert(strstr(packet, ",1,,,,,65535,255,1,15,UNKNOWN,0*") != NULL);
  fputs(packet, stdout);

  ESP32Link_Handle link;
  UART_HandleTypeDef wrong_uart = {USART1, 0U};
  assert(ESP32Link_Init(NULL, &uart) == HAL_ERROR);
  assert(ESP32Link_Init(&link, NULL) == HAL_ERROR);
  assert(ESP32Link_Send(&link, &sample, 0U) == HAL_ERROR && transmits == 0U);
  assert(ESP32Link_Init(&link, &wrong_uart) == HAL_ERROR);
  assert(ESP32Link_Send(&link, &sample, 0U) == HAL_ERROR && transmits == 0U);
  assert(ESP32Link_Send(NULL, &sample, 0U) == HAL_ERROR);
  assert(ESP32Link_Init(&link, &uart) == HAL_OK);
  assert(ESP32Link_Send(&link, &sample, 0U) == HAL_OK && transmits == 1U);
  assert(strncmp(transmitted, "$MESP,2,0,0,", 12U) == 0);
  const HAL_StatusTypeDef failures[] = {HAL_TIMEOUT, HAL_BUSY, HAL_ERROR};
  for (unsigned i = 0U; i < 3U; ++i) {
    transmit_status = failures[i];
    assert(ESP32Link_Send(&link, &sample, 1000U + i * 1000U) == failures[i]);
    assert(transmits == i + 2U && link.failed_packets == i + 1U);
  }
  transmit_status = HAL_OK;
  assert(ESP32Link_Send(&link, &sample, 4000U) == HAL_OK && transmits == 5U);
  assert(strncmp(transmitted, "$MESP,2,4,4000,", 15U) == 0);
  assert(link.sent_packets == 2U && link.next_sequence == 5U);
  assert(ESP32Link_Send(&link, NULL, 5000U) == HAL_ERROR && transmits == 5U);
  assert(link.failed_packets == 4U && link.next_sequence == 6U);
  link.next_sequence = UINT32_MAX;
  assert(ESP32Link_Send(&link, &sample, UINT32_MAX) == HAL_OK && link.next_sequence == 0U);
  assert(ESP32Link_Send(&link, &sample, 0U) == HAL_OK && link.next_sequence == 1U);
  return 0;
}
