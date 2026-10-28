#include "sd_diagnostic.h"
#include "sd_probe.h"
#include "app_log.h"
#include <stdio.h>

static SPI_HandleTypeDef *card_spi;
static GPIO_TypeDef *card_port;
static uint16_t card_pin;
static uint8_t pending, completed;
static SDProbe_Result result;
static uint8_t sector[512];

void SDDiagnostic_Init(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin)
{
  card_spi = spi;
  card_port = port;
  card_pin = pin;
  completed = 0U;
  pending = 1U;
  (void)AppLog_Write("SD read-only test: boot or SD TEST; SD? shows last result. No card writes.\r\n");
}

void SDDiagnostic_Request(void) { pending = 1U; }

void SDDiagnostic_Report(void)
{
  char text[192];
  if (!completed) {
    (void)AppLog_Write("SD: test pending\r\n");
    return;
  }
  (void)snprintf(text, sizeof(text),
      "SD: %s; CMD=%u R1=%02X token=%02X RX=%02X HAL=%u error=0x%08lX elapsed=%lums\r\n",
      SDProbe_StatusName(result.status), (unsigned)result.command,
      (unsigned)result.response, (unsigned)result.token, (unsigned)result.last_rx,
      (unsigned)result.hal_status, (unsigned long)result.hal_error,
      (unsigned long)result.elapsed_ms);
  (void)AppLog_Write(text);
  if (result.status == SD_PROBE_OK) {
    (void)snprintf(text, sizeof(text),
        "SD: %s; sector 0 read, CRC verified; end bytes=%02X %02X (filesystem not tested)\r\n",
        result.block_addressed ? "block addressing" : "byte addressing",
        (unsigned)sector[510], (unsigned)sector[511]);
    (void)AppLog_Write(text);
  }
}

void SDDiagnostic_Process(void)
{
  if (!pending) return;
  pending = 0U;
  (void)AppLog_Write("SD: testing SPI/card, read-only, clock=PCLK2/256\r\n");
  result = SDProbe_Run(card_spi, card_port, card_pin, sector);
  completed = 1U;
  SDDiagnostic_Report();
}

void SDDiagnostic_Display(char *buffer, size_t capacity)
{
  if (pending || !completed) (void)snprintf(buffer, capacity, "SD TEST PENDING");
  else if (result.status == SD_PROBE_OK)
    (void)snprintf(buffer, capacity, "SD READ OK");
  else if (result.status == SD_PROBE_SPI_IO)
    (void)snprintf(buffer, capacity, "SD SPI %s C%u",
        result.hal_status == HAL_TIMEOUT ? "TIME" : "FAIL", (unsigned)result.command);
  else
    (void)snprintf(buffer, capacity, "SD %s C%u",
        SDProbe_StatusName(result.status), (unsigned)result.command);
}
