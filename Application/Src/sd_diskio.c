#include "sd_diskio.h"
#include "app_log.h"
#include "ff.h"
#include "diskio.h"
#include <stddef.h>
#include <stdio.h>

static SD_SPI_Handle card;

void SDDisk_Bind(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin)
{
  SD_SPI_Bind(&card, spi, port, pin);
}

void SDDisk_BeginMount(void)
{
  card.initialized = 0U;
  SD_SPI_BeginOperation(&card);
}

void SDDisk_BeginOperation(void)
{
  SD_SPI_BeginOperation(&card);
}

DSTATUS disk_status(BYTE drive)
{
  return (drive == 0U && card.initialized) ? 0U : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE drive)
{
  if (drive != 0U) return STA_NOINIT;
  return SD_SPI_Init(&card) == HAL_OK ? 0U : STA_NOINIT;
}

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
  if (drive != 0U || buffer == NULL || count == 0U) return RES_PARERR;
  if (disk_status(drive) != 0U) return RES_NOTRDY;
  return SD_SPI_Read(&card, sector, buffer, count) == HAL_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
  if (drive != 0U || buffer == NULL || count == 0U) return RES_PARERR;
  if (disk_status(drive) != 0U) return RES_NOTRDY;
  return SD_SPI_Write(&card, sector, buffer, count) == HAL_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer)
{
  if (drive != 0U) return RES_PARERR;
  if (disk_status(drive) != 0U) return RES_NOTRDY;
  if (command == CTRL_SYNC)
    return SD_SPI_Sync(&card) == HAL_OK ? RES_OK : RES_ERROR;
  if (buffer == NULL) return RES_PARERR;
  switch (command)
  {
    case GET_SECTOR_COUNT: *(LBA_t *)buffer = card.sectors; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buffer = 512U; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buffer = 1U; return RES_OK;
    default: return RES_PARERR;
  }
}

/* Fits one OLED row and preserves the diagnostic firmware's failure names. */
void SDDisk_DiagnosticText(char *buffer, size_t capacity)
{
  (void)snprintf(buffer, capacity, "SD %s C%u",
      SDProbe_StatusName(card.result.status), (unsigned)card.result.command);
}

void SDDisk_Report(void)
{
  char message[160];
  (void)snprintf(message, sizeof(message),
      "SD transport: %s CMD=%u R1=%02X token=%02X HAL=%u flags=%08lX\r\n",
      SDProbe_StatusName(card.result.status), (unsigned)card.result.command,
      (unsigned)card.result.response, (unsigned)card.result.token,
      (unsigned)card.result.hal_status, (unsigned long)card.result.hal_error);
  (void)AppLog_Write(message);
}
