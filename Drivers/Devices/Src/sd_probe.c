#include "sd_probe.h"
#include "sd_spi.h"
#include <stddef.h>

#define IO_MS 20U
#define TOTAL_MS 3000U

typedef struct {
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *port;
  uint16_t pin;
  uint32_t start;
  SDProbe_Result result;
} Probe;

static uint8_t Healthy(Probe *p)
{
  if (p->result.status == SD_PROBE_OK &&
      (uint32_t)(HAL_GetTick() - p->start) >= TOTAL_MS)
    p->result.status = SD_PROBE_DEADLINE;
  return p->result.status == SD_PROBE_OK;
}

static uint8_t Exchange(Probe *p, uint8_t tx)
{
  uint8_t rx = 0xFFU;
  if (!Healthy(p)) return rx;
  HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(p->spi, &tx, &rx, 1U, IO_MS);
  if (status != HAL_OK) {
    p->result.status = SD_PROBE_SPI_IO;
    p->result.hal_status = status;
    p->result.hal_error = p->spi->ErrorCode;
  }
  p->result.last_rx = rx;
  return rx;
}

static void Deselect(Probe *p)
{
  HAL_GPIO_WritePin(p->port, p->pin, GPIO_PIN_SET);
  (void)Exchange(p, 0xFFU);
}

static uint8_t Command(Probe *p, uint8_t cmd, uint32_t arg)
{
  p->result.command = cmd;
  p->result.response = p->result.token = 0xFFU;
  Deselect(p);
  if (!Healthy(p)) return 0xFFU;
  HAL_GPIO_WritePin(p->port, p->pin, GPIO_PIN_RESET);
  /* CMD0 enters SPI mode; do not require a SPI-ready indication beforehand. */
  if (cmd != 0U) {
    uint32_t start = HAL_GetTick();
    while (Exchange(p, 0xFFU) != 0xFFU) {
      if (!Healthy(p)) return 0xFFU;
      if ((uint32_t)(HAL_GetTick() - start) >= 500U) {
        p->result.status = SD_PROBE_READY_TIMEOUT;
        return 0xFFU;
      }
    }
  } else (void)Exchange(p, 0xFFU);
  (void)Exchange(p, (uint8_t)(0x40U | cmd));
  (void)Exchange(p, (uint8_t)(arg >> 24));
  (void)Exchange(p, (uint8_t)(arg >> 16));
  (void)Exchange(p, (uint8_t)(arg >> 8));
  (void)Exchange(p, (uint8_t)arg);
  (void)Exchange(p, cmd == 0U ? 0x95U : (cmd == 8U ? 0x87U : 0x01U));
  for (unsigned i = 0; i < 10U && Healthy(p); ++i) {
    uint8_t rx = Exchange(p, 0xFFU);
    if (Healthy(p) && (rx & 0x80U) == 0U) {
      p->result.response = rx;
      return rx;
    }
  }
  if (Healthy(p)) p->result.status = SD_PROBE_NO_RESPONSE;
  return 0xFFU;
}

static uint8_t Expect(Probe *p, uint8_t cmd, uint32_t arg, uint8_t expected)
{
  uint8_t response = Command(p, cmd, arg);
  if (Healthy(p) && response != expected) p->result.status = SD_PROBE_RESPONSE;
  return Healthy(p);
}

static uint16_t Crc16(uint16_t crc, uint8_t byte)
{
  crc ^= (uint16_t)byte << 8;
  for (unsigned i = 0U; i < 8U; ++i)
    crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
  return crc;
}

static void ReceiveBlock(Probe *p, uint8_t *data, unsigned size)
{
  uint32_t start = HAL_GetTick();
  uint16_t crc = 0U, received_crc;
  do {
    p->result.token = Exchange(p, 0xFFU);
    if (!Healthy(p)) return;
    if (p->result.token != 0xFFU) break;
    if ((uint32_t)(HAL_GetTick() - start) >= 250U) {
      p->result.status = SD_PROBE_TOKEN_TIMEOUT;
      return;
    }
  } while (Healthy(p));
  if (p->result.token != 0xFEU) { p->result.status = SD_PROBE_TOKEN; return; }
  for (unsigned i = 0U; i < size; ++i) {
    data[i] = Exchange(p, 0xFFU);
    if (!Healthy(p)) return;
    crc = Crc16(crc, data[i]);
  }
  received_crc = (uint16_t)Exchange(p, 0xFFU) << 8;
  received_crc |= Exchange(p, 0xFFU);
  if (Healthy(p) && crc != received_crc) p->result.status = SD_PROBE_CRC;
}

SDProbe_Result SDProbe_Run(SPI_HandleTypeDef *spi, GPIO_TypeDef *port,
                           uint16_t pin, uint8_t sector[512])
{
  Probe p = {.spi = spi, .port = port, .pin = pin, .start = HAL_GetTick(),
             .result = {.command = 0xFFU, .response = 0xFFU,
                        .token = 0xFFU, .last_rx = 0xFFU}};
  uint8_t response, v2, bytes[4];
  uint32_t start;
  if (spi == NULL || port == NULL || pin == 0U || sector == NULL) {
    p.result.status = SD_PROBE_ARGUMENT;
    return p.result;
  }
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
  spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  p.result.hal_status = HAL_SPI_Init(spi);
  if (p.result.hal_status != HAL_OK) {
    p.result.status = SD_PROBE_SPI_INIT;
    p.result.hal_error = spi->ErrorCode;
    goto done;
  }
  HAL_Delay(20U);
  for (unsigned i = 0U; i < 10U; ++i) (void)Exchange(&p, 0xFFU);
  if (!Healthy(&p)) goto done;
  start = HAL_GetTick();
  do {
    response = Command(&p, 0U, 0U);
    if (Healthy(&p) && response == 1U) break;
    if (p.result.status != SD_PROBE_OK && p.result.status != SD_PROBE_NO_RESPONSE)
      goto done;
    if ((uint32_t)(HAL_GetTick() - start) >= 300U) {
      if (p.result.status == SD_PROBE_OK) p.result.status = SD_PROBE_RESPONSE;
      goto done;
    }
    p.result.status = SD_PROBE_OK; /* Retry CMD0 only, within a fixed deadline. */
    Deselect(&p);
    HAL_Delay(10U);
  } while (Healthy(&p));
  if (!Healthy(&p)) goto done;
  response = Command(&p, 8U, 0x1AAU);
  if (!Healthy(&p)) goto done;
  v2 = response == 1U;
  if (v2) {
    for (unsigned i = 0; i < 4U; ++i) bytes[i] = Exchange(&p, 0xFFU);
    if (!Healthy(&p)) goto done;
    if (bytes[0] != 0U || bytes[1] != 0U || bytes[2] != 1U || bytes[3] != 0xAAU) {
      p.result.status = SD_PROBE_ECHO;
      goto done;
    }
  } else if (response != 5U) {
    p.result.status = SD_PROBE_RESPONSE;
    goto done;
  }
  start = HAL_GetTick();
  do {
    response = Command(&p, 55U, 0U);
    if (!Healthy(&p)) goto done;
    if (response > 1U) { p.result.status = SD_PROBE_RESPONSE; goto done; }
    response = Command(&p, 41U, v2 ? 0x40000000U : 0U);
    if (!Healthy(&p)) goto done;
    if (response == 0U) break;
    if (response != 1U) { p.result.status = SD_PROBE_RESPONSE; goto done; }
    if ((uint32_t)(HAL_GetTick() - start) >= 1500U) {
      p.result.status = SD_PROBE_INIT_TIMEOUT;
      goto done;
    }
    HAL_Delay(1U);
  } while (Healthy(&p));
  if (!Healthy(&p) || !Expect(&p, 58U, 0U, 0U)) goto done;
  for (unsigned i = 0; i < 4U; ++i) bytes[i] = Exchange(&p, 0xFFU);
  if (!Healthy(&p)) goto done;
  if ((bytes[0] & 0x80U) == 0U || (bytes[1] & 0x30U) == 0U) {
    p.result.status = SD_PROBE_VOLTAGE;
    goto done;
  }
  p.result.block_addressed = (uint8_t)(v2 && (bytes[0] & 0x40U) != 0U);
  if (!p.result.block_addressed && !Expect(&p, 16U, 512U, 0U)) goto done;
  if (!Expect(&p, 17U, 0U, 0U)) goto done;
  ReceiveBlock(&p, sector, 512U);
done:
  /* Always release CS; preserve the failing command/response and received byte. */
  {
    uint8_t last_rx = p.result.last_rx;
    Deselect(&p);
    p.result.last_rx = last_rx;
  }
  p.result.elapsed_ms = HAL_GetTick() - p.start;
  return p.result;
}

const char *SDProbe_StatusName(SDProbe_Status status)
{
  switch (status) {
    case SD_PROBE_OK: return "READ OK";
    case SD_PROBE_ARGUMENT: return "BAD HANDLE";
    case SD_PROBE_SPI_INIT: return "SPI INIT";
    case SD_PROBE_SPI_IO: return "SPI IO";
    case SD_PROBE_DEADLINE: return "DEADLINE";
    case SD_PROBE_READY_TIMEOUT: return "BUSY";
    case SD_PROBE_NO_RESPONSE: return "NO R1";
    case SD_PROBE_RESPONSE: return "BAD R1";
    case SD_PROBE_ECHO: return "BAD ECHO";
    case SD_PROBE_INIT_TIMEOUT: return "INIT TIME";
    case SD_PROBE_VOLTAGE: return "BAD OCR";
    case SD_PROBE_TOKEN_TIMEOUT: return "NO TOKEN";
    case SD_PROBE_TOKEN: return "BAD TOKEN";
    case SD_PROBE_CRC: return "DATA CRC";
    case SD_PROBE_CAPACITY: return "CAPACITY";
    case SD_PROBE_WRITE_REJECT: return "WRITE REJECT";
    case SD_PROBE_WRITE_STATUS: return "WRITE STATUS";
    default: return "UNKNOWN";
  }
}

void SD_SPI_Bind(SD_SPI_Handle *card, SPI_HandleTypeDef *spi,
                 GPIO_TypeDef *port, uint16_t pin)
{
  *card = (SD_SPI_Handle){.spi = spi, .cs_port = port, .cs_pin = pin};
}

void SD_SPI_BeginOperation(SD_SPI_Handle *card)
{
  card->operation_start = HAL_GetTick();
  card->result = (SDProbe_Result){.command = 255U, .response = 255U,
                                  .token = 255U, .last_rx = 255U,
                                  .block_addressed = card->block_addressed};
}

static Probe CardProbe(SD_SPI_Handle *card)
{
  return (Probe){.spi = card->spi, .port = card->cs_port, .pin = card->cs_pin,
                 .start = card->operation_start, .result = card->result};
}

static HAL_StatusTypeDef FinishCard(SD_SPI_Handle *card, Probe *p)
{
  uint8_t rx = p->result.last_rx;
  Deselect(p);
  (void)Healthy(p);
  p->result.last_rx = rx;
  p->result.elapsed_ms = HAL_GetTick() - p->start;
  card->result = p->result;
  if (p->result.status != SD_PROBE_OK) {
    card->initialized = 0U;
    return HAL_ERROR;
  }
  return HAL_OK;
}

HAL_StatusTypeDef SD_SPI_Init(SD_SPI_Handle *card)
{
  uint8_t scratch[512], csd[16];
  uint64_t sectors = 0U;
  card->initialized = 0U;
  card->sectors = 0U;
  SD_SPI_BeginOperation(card);
  /* Reuse exactly the initialization and read path verified on the board. */
  card->result = SDProbe_Run(card->spi, card->cs_port, card->cs_pin, scratch);
  if (card->result.status != SD_PROBE_OK) return HAL_ERROR;
  card->block_addressed = card->result.block_addressed;
  Probe p = CardProbe(card);
  if (Expect(&p, 9U, 0U, 0U)) ReceiveBlock(&p, csd, sizeof(csd));
  if (!Healthy(&p)) return FinishCard(card, &p);
  if ((csd[0] >> 6) == 1U) {
    uint32_t size = ((uint32_t)(csd[7] & 0x3FU) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
    sectors = ((uint64_t)size + 1U) * 1024U;
  } else if ((csd[0] >> 6) == 0U) {
    uint32_t size = ((uint32_t)(csd[6] & 3U) << 10) | ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
    unsigned shift = (csd[5] & 15U) + ((csd[9] & 3U) << 1) + (csd[10] >> 7) + 2U;
    if (shift >= 9U && shift <= 31U) sectors = ((uint64_t)size + 1U) << (shift - 9U);
  }
  if (sectors == 0U || sectors > UINT32_MAX) p.result.status = SD_PROBE_CAPACITY;
  if (FinishCard(card, &p) != HAL_OK) return HAL_ERROR;
  card->sectors = (uint32_t)sectors;
  card->initialized = 1U;
  return HAL_OK;
}

static uint8_t ValidRequest(SD_SPI_Handle *card, uint32_t sector, uint32_t count)
{
  return card->initialized && count != 0U && sector < card->sectors &&
      count <= card->sectors - sector &&
      (card->block_addressed || sector + count - 1U <= UINT32_MAX / 512U);
}

HAL_StatusTypeDef SD_SPI_Read(SD_SPI_Handle *card, uint32_t sector, uint8_t *data, uint32_t count)
{
  if (data == NULL || !ValidRequest(card, sector, count)) {
    card->result.status = SD_PROBE_ARGUMENT;
    return HAL_ERROR;
  }
  Probe p = CardProbe(card);
  for (uint32_t i = 0U; i < count && Healthy(&p); ++i) {
    uint32_t address = card->block_addressed ? sector + i : (sector + i) * 512U;
    if (Expect(&p, 17U, address, 0U)) ReceiveBlock(&p, data + i * 512U, 512U);
  }
  return FinishCard(card, &p);
}

static void WaitReady(Probe *p)
{
  uint32_t start = HAL_GetTick();
  while (Healthy(p)) {
    if (Exchange(p, 0xFFU) == 0xFFU) return;
    if (Healthy(p) && (uint32_t)(HAL_GetTick() - start) >= 500U)
      p->result.status = SD_PROBE_READY_TIMEOUT;
  }
}

HAL_StatusTypeDef SD_SPI_Write(SD_SPI_Handle *card, uint32_t sector, const uint8_t *data, uint32_t count)
{
  if (data == NULL || !ValidRequest(card, sector, count)) {
    card->result.status = SD_PROBE_ARGUMENT;
    return HAL_ERROR;
  }
  Probe p = CardProbe(card);
  for (uint32_t i = 0U; i < count && Healthy(&p); ++i) {
    uint32_t address = card->block_addressed ? sector + i : (sector + i) * 512U;
    uint16_t crc = 0U;
    if (!Expect(&p, 24U, address, 0U)) break;
    (void)Exchange(&p, 0xFFU);
    (void)Exchange(&p, 0xFEU);
    for (unsigned j = 0U; j < 512U && Healthy(&p); ++j) {
      uint8_t byte = data[i * 512U + j];
      crc = Crc16(crc, byte);
      (void)Exchange(&p, byte);
    }
    (void)Exchange(&p, (uint8_t)(crc >> 8));
    (void)Exchange(&p, (uint8_t)crc);
    p.result.token = Exchange(&p, 0xFFU);
    if (!Healthy(&p)) break;
    if ((p.result.token & 0x1FU) != 0x05U) { p.result.status = SD_PROBE_WRITE_REJECT; break; }
    WaitReady(&p);
    if (!Healthy(&p) || !Expect(&p, 13U, 0U, 0U)) break;
    uint8_t status = Exchange(&p, 0xFFU);
    if (Healthy(&p) && status != 0U) {
      p.result.token = status;
      p.result.status = SD_PROBE_WRITE_STATUS;
    }
  }
  return FinishCard(card, &p);
}

HAL_StatusTypeDef SD_SPI_Sync(SD_SPI_Handle *card)
{
  if (!card->initialized) return HAL_ERROR;
  Probe p = CardProbe(card);
  HAL_GPIO_WritePin(p.port, p.pin, GPIO_PIN_RESET);
  WaitReady(&p);
  return FinishCard(card, &p);
}
