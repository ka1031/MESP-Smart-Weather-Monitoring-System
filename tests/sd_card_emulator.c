#include "sd_card_emulator.h"
#include "app_log.h"
#include <assert.h>
#include <string.h>

SPI_HandleTypeDef test_spi;
int card_absent, card_legacy, card_byte_addressed, card_bad_echo;
int card_read_error, card_write_reject, card_busy, card_status_error, card_hal_error;
unsigned int card_write_commands, card_read_commands, card_init_commands;
uint32_t card_last_address;
char sd_output[8192];
static FILE *disk;
static uint32_t sectors;
static uint64_t tick_us;
static unsigned int high_clock_bytes;
static int selected, idle, app_command;
static uint8_t command[6], queue[520], written[512];
static unsigned int command_length, queue_in, queue_out, write_left;
static int awaiting_token;
static uint16_t write_crc;

void CardEmulator_ResetFaults(void)
{
  card_absent = card_legacy = card_byte_addressed = card_bad_echo = 0;
  card_read_error = card_write_reject = card_busy = card_status_error = card_hal_error = 0;
  sd_output[0] = '\0';
}

void CardEmulator_Open(const char *path)
{
  disk = fopen(path, "r+b"); assert(disk != NULL);
  assert(fseek(disk, 0, SEEK_END) == 0);
  long bytes = ftell(disk); assert(bytes > 0 && bytes % 512 == 0);
  sectors = (uint32_t)(bytes / 512);
  CardEmulator_ResetFaults();
  idle = 1; selected = app_command = 0;
  command_length = queue_in = queue_out = write_left = high_clock_bytes = 0U;
  awaiting_token = 0;
}

void CardEmulator_Close(void) { assert(fclose(disk) == 0); disk = NULL; }

HAL_StatusTypeDef AppLog_Write(const char *text)
{
  assert(strlen(sd_output) + strlen(text) < sizeof(sd_output));
  strcat(sd_output, text);
  return HAL_OK;
}

uint32_t HAL_GetTick(void) { tick_us += 10U; return (uint32_t)(tick_us / 1000U); }
void HAL_Delay(uint32_t ms) { tick_us += (uint64_t)ms * 1000U; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi)
{
  assert(spi == &test_spi);
  assert(spi->Init.BaudRatePrescaler == 256U || spi->Init.BaudRatePrescaler == 32U);
  return card_hal_error ? HAL_ERROR : HAL_OK;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState value)
{
  assert(port == GPIOA && pin == GPIO_PIN_4);
  selected = value == GPIO_PIN_RESET;
  if (!selected)
  {
    command_length = queue_in = queue_out = write_left = 0U;
    awaiting_token = 0;
  }
}

static void Push(uint8_t byte) { assert(queue_in < sizeof(queue)); queue[queue_in++] = byte; }

static uint16_t DataCrc(const uint8_t *data, unsigned size)
{
  uint16_t crc = 0U;
  while (size--) {
    crc ^= (uint16_t)*data++ << 8;
    for (unsigned bit = 0U; bit < 8U; ++bit)
      crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
  }
  return crc;
}
static void PushCrc(const uint8_t *data, unsigned size)
{
  uint16_t crc = DataCrc(data, size);
  Push((uint8_t)(crc >> 8)); Push((uint8_t)crc);
}

static uint32_t Sector(uint32_t address)
{
  if (card_legacy || card_byte_addressed)
  {
    assert(address % 512U == 0U);
    address /= 512U;
  }
  assert(address < sectors);
  return address;
}

static void Execute(void)
{
  uint8_t cmd = command[0] & 0x3FU;
  uint32_t arg = (uint32_t)command[1] << 24 | (uint32_t)command[2] << 16 |
                 (uint32_t)command[3] << 8 | command[4];
  int was_app = app_command;
  app_command = 0;
  switch (cmd)
  {
    case 0:
      assert(command[5] == 0x95U && high_clock_bytes >= 10U);
      assert(test_spi.Init.BaudRatePrescaler == 256U);
      ++card_init_commands; idle = 1; Push(1U); break;
    case 8:
      assert(arg == 0x1AAU && command[5] == 0x87U);
      Push(card_legacy ? 5U : 1U);
      if (!card_legacy) { Push(0); Push(0); Push(1); Push(card_bad_echo ? 0 : 0xAA); }
      break;
    case 55: app_command = 1; Push((uint8_t)idle); break;
    case 41:
      assert(was_app && arg == (card_legacy ? 0U : 0x40000000U));
      idle = 0; Push(0); break;
    case 58:
      Push(0); Push(card_legacy || card_byte_addressed ? 0x80 : 0xC0);
      Push(0x30); Push(0); Push(0); break;
    case 16: assert((card_legacy || card_byte_addressed) && arg == 512U); Push(0); break;
    case 9:
    {
      uint8_t csd[16] = {0};
      if (card_legacy || card_byte_addressed)
      {
        unsigned int mult = 0;
        while ((sectors >> (mult + 2U)) > 4096U) ++mult;
        assert(mult <= 7U);
        uint32_t size = (sectors >> (mult + 2U)) - 1U;
        csd[5] = 9; csd[6] = (uint8_t)(size >> 10);
        csd[7] = (uint8_t)(size >> 2); csd[8] = (uint8_t)(size << 6);
        csd[9] = (uint8_t)(mult >> 1); csd[10] = (uint8_t)(mult << 7);
      }
      else
      {
        assert(sectors % 1024U == 0U);
        uint32_t size = sectors / 1024U - 1U;
        csd[0] = 0x40; csd[7] = (uint8_t)(size >> 16);
        csd[8] = (uint8_t)(size >> 8); csd[9] = (uint8_t)size;
      }
      Push(0); Push(0xFE);
      for (size_t i = 0; i < sizeof(csd); ++i) Push(csd[i]);
      PushCrc(csd, sizeof(csd)); break;
    }
    case 17:
    {
      ++card_read_commands; card_last_address = arg;
      Push(0);
      if (card_read_error) { Push(0x0B); break; }
      Push(0xFE);
      assert(fseek(disk, (long)Sector(arg) * 512L, SEEK_SET) == 0);
      assert(fread(queue + queue_in, 1, 512, disk) == 512);
      queue_in += 512U; PushCrc(queue + 2U, 512U); break;
    }
    case 24:
      ++card_write_commands; card_last_address = arg; (void)Sector(arg);
      Push(0); awaiting_token = 1; break;
    case 13: Push(0); Push(card_status_error ? 0x20 : 0); break;
    default: assert(!"Unexpected SPI command");
  }
}

static uint8_t Transfer(uint8_t tx)
{
  tick_us += test_spi.Init.BaudRatePrescaler == 256U ? 29U : 4U;
  if (card_absent) return 0xFF;
  if (!selected) { if (tx == 0xFF) ++high_clock_bytes; return 0xFF; }
  if (queue_out < queue_in) return queue[queue_out++];
  queue_in = queue_out = 0U;
  if (write_left != 0U)
  {
    if (write_left > 2U) written[514U - write_left] = tx;
    else if (write_left == 2U) write_crc = (uint16_t)tx << 8;
    else write_crc |= tx;
    if (--write_left == 0U)
    {
      assert(write_crc == DataCrc(written, sizeof(written)));
      if (!card_write_reject)
      {
        assert(fseek(disk, (long)Sector(card_last_address) * 512L, SEEK_SET) == 0);
        assert(fwrite(written, 1, 512, disk) == 512);
        assert(fflush(disk) == 0);
      }
      Push(card_write_reject ? 0x0D : 0x05);
      Push(0); Push(0xFF); /* short programming busy interval */
    }
    return 0xFF;
  }
  if (awaiting_token)
  {
    if (tx == 0xFE) { awaiting_token = 0; write_left = 514U; }
    else assert(tx == 0xFF);
    return 0xFF;
  }
  if (card_busy) return 0;
  if (command_length != 0U || (tx & 0xC0U) == 0x40U)
  {
    command[command_length++] = tx;
    if (command_length == 6U) { command_length = 0U; Execute(); }
  }
  return 0xFF;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx,
                                          uint8_t *rx, uint16_t size, uint32_t timeout)
{
  assert(spi == &test_spi && timeout > 0U && timeout <= 20U);
  if (card_hal_error) return HAL_TIMEOUT;
  for (uint16_t i = 0; i < size; ++i) rx[i] = Transfer(tx[i]);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *tx,
                                   uint16_t size, uint32_t timeout)
{
  assert(spi == &test_spi && size == 512U && timeout <= 20U);
  if (card_hal_error) return HAL_TIMEOUT;
  for (uint16_t i = 0; i < size; ++i) (void)Transfer(tx[i]);
  return HAL_OK;
}
