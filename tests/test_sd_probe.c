#include "sd_probe.h"
#include "sd_diagnostic.h"
#include "app_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum Fault {NONE, ABSENT, MISO_LOW, INIT_FAIL, IO_FAIL, COMMAND_IO_FAIL,
            BUSY, ECHO, INIT_STUCK, OCR, BAD_R1, TOKEN_WAIT, TOKEN_ERROR,
            BAD_CRC, SLOW, DELAYED_RESET};
static enum Fault fault;
static SPI_HandleTypeDef spi;
static uint32_t tick;
static int selected, legacy, byte_addressed;
static unsigned high_clocks, command_len, qin, qout, reset_count, read_count;
static uint8_t command[6], queue[520];
static char output[4096];

uint32_t HAL_GetTick(void) { return tick++; }
void HAL_Delay(uint32_t ms) { tick += ms; }
HAL_StatusTypeDef AppLog_Write(const char *text)
{
  assert(strlen(output) + strlen(text) < sizeof(output));
  strcat(output, text);
  return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *s)
{
  assert(s == &spi && s->Init.BaudRatePrescaler == 256U);
  s->ErrorCode = fault == INIT_FAIL ? 0x10U : 0U;
  return fault == INIT_FAIL ? HAL_ERROR : HAL_OK;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  assert(port == GPIOA && pin == GPIO_PIN_4);
  selected = state == GPIO_PIN_RESET;
  if (!selected) command_len = qin = qout = 0U;
}
static void Push(uint8_t value) { assert(qin < sizeof(queue)); queue[qin++] = value; }
static void Execute(void)
{
  uint8_t cmd = command[0] & 0x3FU;
  uint32_t arg = (uint32_t)command[1] << 24 | (uint32_t)command[2] << 16 |
                 (uint32_t)command[3] << 8 | command[4];
  switch (cmd) {
    case 0:
      assert(high_clocks >= 10U && arg == 0U && command[5] == 0x95U);
      ++reset_count;
      if (fault == DELAYED_RESET && reset_count < 3U) break;
      Push(1U); break;
    case 8:
      assert(arg == 0x1AAU && command[5] == 0x87U);
      Push(legacy ? 5U : 1U);
      if (!legacy) { Push(0); Push(0); Push(1); Push(fault == ECHO ? 0 : 0xAA); }
      break;
    case 55: Push(1); break;
    case 41:
      assert(arg == (legacy ? 0U : 0x40000000U));
      Push(fault == INIT_STUCK ? 1U : 0U); break;
    case 58:
      Push(0); Push(fault == OCR ? 0 : (legacy || byte_addressed ? 0x80 : 0xC0));
      Push(0x30); Push(0); Push(0); break;
    case 16: assert((legacy || byte_addressed) && arg == 512U); Push(0); break;
    case 17:
      assert(arg == 0U); ++read_count;
      Push(fault == BAD_R1 ? 0x20 : 0);
      if (fault == BAD_R1 || fault == TOKEN_WAIT) break;
      Push(fault == TOKEN_ERROR ? 0x0B : 0xFE);
      if (fault == TOKEN_ERROR) break;
      for (unsigned i = 0U; i < 512U; ++i) Push((uint8_t)i);
      /* CRC-CCITT, init=0, for bytes 0..255 repeated twice (independent vector). */
      Push(0x40); Push(fault == BAD_CRC ? 0 : 0xDA); break;
    default:
      /* A write/erase command is a test failure, not an emulated operation. */
      assert(!"Unexpected or destructive command");
  }
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *s, uint8_t *tx,
                                          uint8_t *rx, uint16_t size, uint32_t timeout)
{
  assert(s == &spi && size == 1U && timeout <= 20U && timeout > 0U);
  if (fault == IO_FAIL || (fault == COMMAND_IO_FAIL && selected)) {
    s->ErrorCode = 0x20U;
    return HAL_TIMEOUT;
  }
  if (fault == SLOW) tick += 8U;
  *rx = 0xFFU;
  if (!selected) { if (*tx == 0xFFU) ++high_clocks; return HAL_OK; }
  if (fault == ABSENT) return HAL_OK;
  if (fault == MISO_LOW) { *rx = 0U; return HAL_OK; }
  if (qout < qin) { *rx = queue[qout++]; return HAL_OK; }
  qin = qout = 0U;
  if (fault == BUSY && reset_count != 0U) { *rx = 0U; return HAL_OK; }
  if (command_len != 0U || (*tx & 0xC0U) == 0x40U) {
    command[command_len++] = *tx;
    if (command_len == 6U) { command_len = 0U; Execute(); }
  }
  return HAL_OK;
}

static void Reset(enum Fault f)
{
  fault = f; tick = 0; selected = legacy = byte_addressed = 0;
  high_clocks = command_len = qin = qout = reset_count = read_count = 0U;
  output[0] = '\0';
  spi = (SPI_HandleTypeDef){0};
}
static SDProbe_Result Run(void)
{
  uint8_t sector[512] = {0};
  SDProbe_Result result = SDProbe_Run(&spi, GPIOA, GPIO_PIN_4, sector);
  assert(!selected && result.elapsed_ms <= 3050U);
  if (result.status == SD_PROBE_OK) {
    assert(read_count == 1U && result.command == 17U && result.token == 0xFEU);
    for (unsigned i = 0; i < 512U; ++i) assert(sector[i] == (uint8_t)i);
  }
  return result;
}

int main(void)
{
  SDProbe_Result r;
  Reset(NONE); r = Run(); assert(r.status == SD_PROBE_OK && r.block_addressed);
  Reset(NONE); legacy = 1; r = Run(); assert(r.status == SD_PROBE_OK && !r.block_addressed);
  Reset(NONE); byte_addressed = 1; r = Run(); assert(r.status == SD_PROBE_OK && !r.block_addressed);
  Reset(NONE); tick = UINT32_MAX - 50U; assert(Run().status == SD_PROBE_OK);
  Reset(DELAYED_RESET); assert(Run().status == SD_PROBE_OK && reset_count == 3U);
  Reset(ABSENT); r = Run();
  assert(r.status == SD_PROBE_NO_RESPONSE && r.command == 0U && r.response == 0xFFU && r.last_rx == 0xFFU);
  Reset(MISO_LOW); r = Run(); assert(r.status == SD_PROBE_RESPONSE && r.last_rx == 0U);
  Reset(INIT_FAIL); r = Run(); assert(r.status == SD_PROBE_SPI_INIT && r.hal_error == 0x10U);
  Reset(IO_FAIL); r = Run(); assert(r.status == SD_PROBE_SPI_IO && r.command == 255U && r.hal_status == HAL_TIMEOUT);
  Reset(COMMAND_IO_FAIL); r = Run();
  assert(r.status == SD_PROBE_SPI_IO && r.command == 0U && r.hal_error == 0x20U);
  Reset(BUSY); r = Run(); assert(r.status == SD_PROBE_READY_TIMEOUT && r.command == 8U);
  Reset(ECHO); assert(Run().status == SD_PROBE_ECHO);
  Reset(INIT_STUCK); assert(Run().status == SD_PROBE_INIT_TIMEOUT);
  Reset(OCR); assert(Run().status == SD_PROBE_VOLTAGE);
  Reset(BAD_R1); r = Run(); assert(r.status == SD_PROBE_RESPONSE && r.response == 0x20U);
  Reset(TOKEN_WAIT); assert(Run().status == SD_PROBE_TOKEN_TIMEOUT);
  Reset(TOKEN_ERROR); r = Run(); assert(r.status == SD_PROBE_TOKEN && r.token == 0x0BU);
  Reset(BAD_CRC); assert(Run().status == SD_PROBE_CRC);
  Reset(SLOW); assert(Run().status == SD_PROBE_DEADLINE);
  Reset(NONE); assert(SDProbe_Run(NULL, GPIOA, GPIO_PIN_4, NULL).status == SD_PROBE_ARGUMENT);

  /* Real application wrapper: boot, latched result, explicit retry and report. */
  Reset(ABSENT);
  char display[24];
  SDDiagnostic_Init(&spi, GPIOA, GPIO_PIN_4);
  SDDiagnostic_Display(display, sizeof(display)); assert(strcmp(display, "SD TEST PENDING") == 0);
  SDDiagnostic_Process();
  SDDiagnostic_Display(display, sizeof(display)); assert(strcmp(display, "SD NO R1 C0") == 0);
  assert(strstr(output, "CMD=0 R1=FF") != NULL);
  uint32_t old_tick = tick;
  SDDiagnostic_Process(); assert(tick == old_tick); /* No automatic repeated blocking. */
  fault = NONE;
  SDDiagnostic_Request(); SDDiagnostic_Process();
  SDDiagnostic_Display(display, sizeof(display)); assert(strcmp(display, "SD READ OK") == 0);
  assert(strstr(output, "CRC verified") != NULL && read_count == 1U);
  SDDiagnostic_Report(); assert(read_count == 1U);
  fault = COMMAND_IO_FAIL;
  SDDiagnostic_Request(); SDDiagnostic_Process();
  SDDiagnostic_Display(display, sizeof(display)); assert(strcmp(display, "SD SPI TIME C0") == 0);
  puts("Read-only SD probe: SDHC/SDSC, CRC, retries, timeouts, faults and diagnostic integration passed");
  return 0;
}
