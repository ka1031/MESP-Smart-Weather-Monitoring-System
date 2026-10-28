#include "sd_card_emulator.h"
#include "sd_diskio.h"
#include "sd_logger.h"
#include "weather_csv.h"
#include "ff.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static WeatherData sample = {
  .temperature_c = -0.25F, .humidity_percent = 62.5F, .pressure_hpa = 1008.25F,
  .illuminance_lux = 123.75F, .rain_raw = 3000U, .rain_percent = 27U, .rain_wet = 0U,
  .timestamp = {2026U, 10U, 28U, 14U, 32U, 5U}, .valid_flags = 15U
};

static void ProtocolTests(void)
{
  SD_SPI_Handle card;
  uint8_t original[1024], pattern[1024], received[1024];
  memset(pattern, 0xA5, sizeof(pattern));
  for (int kind = 0; kind < 3; ++kind)
  {
    card_legacy = kind == 1; card_byte_addressed = kind == 2;
    SD_SPI_Bind(&card, &test_spi, GPIOA, GPIO_PIN_4);
    assert(SD_SPI_Init(&card) == HAL_OK);
    assert(card.block_addressed == (kind == 0));
    uint32_t sector = card.sectors - 2U;
    SD_SPI_BeginOperation(&card);
    assert(SD_SPI_Read(&card, sector, original, 2U) == HAL_OK);
    assert(card_last_address == (kind == 0 ? sector + 1U : (sector + 1U) * 512U));
    assert(SD_SPI_Write(&card, sector, pattern, 2U) == HAL_OK);
    assert(SD_SPI_Read(&card, sector, received, 2U) == HAL_OK);
    assert(memcmp(pattern, received, sizeof(pattern)) == 0);
    assert(SD_SPI_Write(&card, sector, original, 2U) == HAL_OK);
    unsigned int before = card_write_commands;
    assert(SD_SPI_Write(&card, card.sectors, pattern, 1U) == HAL_ERROR);
    assert(SD_SPI_Write(&card, card.sectors - 1U, pattern, 2U) == HAL_ERROR);
    assert(SD_SPI_Write(&card, 0U, pattern, 0U) == HAL_ERROR);
    assert(card_write_commands == before);
  }
  CardEmulator_ResetFaults();
  card_absent = 1;
  assert(SD_SPI_Init(&card) == HAL_ERROR && !card.initialized);
  card_absent = 0; card_bad_echo = 1;
  assert(SD_SPI_Init(&card) == HAL_ERROR && !card.initialized);
  card_bad_echo = 0;
  assert(SD_SPI_Init(&card) == HAL_OK);
  card_read_error = 1;
  assert(SD_SPI_Read(&card, 0U, received, 1U) == HAL_ERROR && !card.initialized);
  card_read_error = 0;
  assert(SD_SPI_Init(&card) == HAL_OK);
  card_write_reject = 1;
  assert(SD_SPI_Write(&card, card.sectors - 1U, pattern, 1U) == HAL_ERROR && !card.initialized);
  card_write_reject = 0;
  assert(SD_SPI_Init(&card) == HAL_OK);
  card_status_error = 1;
  assert(SD_SPI_Write(&card, card.sectors - 1U, original + 512, 1U) == HAL_ERROR);
  card_status_error = 0;
  assert(SD_SPI_Init(&card) == HAL_OK);
  uint32_t start = HAL_GetTick();
  card_busy = 1;
  assert(SD_SPI_Sync(&card) == HAL_ERROR && !card.initialized);
  assert(HAL_GetTick() - start < 2100U);
  card_busy = 0;
  assert(SD_SPI_Init(&card) == HAL_OK);
  SD_SPI_BeginOperation(&card);
  HAL_Delay(3001U);
  assert(SD_SPI_Read(&card, 0U, received, 1U) == HAL_ERROR && !card.initialized);
  assert(SD_SPI_Init(&card) == HAL_OK);
  card_hal_error = 1;
  assert(SD_SPI_Read(&card, 0U, received, 1U) == HAL_ERROR && !card.initialized);
  CardEmulator_ResetFaults();
  puts("SD SPI: SDHC, SDSC v1/v2, addressing, multi-sector I/O, timeouts and error checks passed");
}

static void CSVTests(void)
{
  char output[256];
  assert(WeatherCSV_Format(output, sizeof(output), &sample, 1234U) > 0U);
  assert(strcmp(output, "1234,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15\r\n") == 0);
  WeatherData invalid = sample; invalid.valid_flags = 0U;
  assert(WeatherCSV_Format(output, sizeof(output), &invalid, 0U) > 0U);
  assert(strcmp(output, "0,,0,,,,,,,,0\r\n") == 0);
  invalid.valid_flags = 1U; invalid.temperature_c = NAN;
  assert(WeatherCSV_Format(output, sizeof(output), &invalid, 0U) > 0U);
  assert(strstr(output, "0,,0,,62.50,1008.25,") == output);
  invalid.temperature_c = INFINITY;
  assert(WeatherCSV_Format(output, sizeof(output), &invalid, 0U) > 0U);
  assert(strstr(output, "inf") == NULL);
  assert(WeatherCSV_Format(output, 8U, &sample, 0U) == 0U);
}

static void CheckFile(const char *name, const char *rows_text)
{
  FATFS fs; FIL file; char text[1024]; UINT read;
  SDDisk_BeginOperation();
  assert(f_mount(&fs, "", 1U) == FR_OK);
  assert(f_open(&file, name, FA_READ) == FR_OK);
  assert(f_read(&file, text, sizeof(text) - 1U, &read) == FR_OK);
  text[read] = '\0';
  assert(strncmp(text, WEATHER_CSV_HEADER, strlen(WEATHER_CSV_HEADER)) == 0);
  assert(strcmp(text + strlen(WEATHER_CSV_HEADER), rows_text) == 0);
  assert(f_close(&file) == FR_OK);
  assert(f_mount(NULL, "", 0U) == FR_OK);
}

static void LoggingTests(void)
{
  char display[24];
  SDLogger_Init(&test_spi, GPIOA, GPIO_PIN_4);
  card_absent = 1;
  SDLogger_Process(&sample, 0U);
  assert(SDLogger_GetState() == SD_LOGGER_ERROR);
  assert(strstr(sd_output, "card not ready") != NULL);
  SDLogger_DisplayText(display, sizeof(display));
  assert(strcmp(display, "SD NO R1 C0") == 0);
  unsigned int attempts = card_init_commands;
  card_absent = 0;
  SDLogger_Process(&sample, 5000U);
  assert(card_init_commands == attempts); /* No retries without START. */
  SDLogger_Start();
  SDLogger_Process(&sample, 1000U);
  assert(SDLogger_GetState() == SD_LOGGER_ACTIVE);
  SDLogger_DisplayText(display, sizeof(display)); assert(strcmp(display, "SD LOG 1") == 0);
  SDLogger_Process(&sample, 5999U);
  SDLogger_DisplayText(display, sizeof(display)); assert(strcmp(display, "SD LOG 1") == 0);
  WeatherData invalid = sample; invalid.valid_flags = 0U;
  SDLogger_Process(&invalid, 6000U);
  SDLogger_DisplayText(display, sizeof(display)); assert(strcmp(display, "SD LOG 2") == 0);
  SDLogger_Start(); /* Already active must not discard/reopen a file. */
  SDLogger_Stop();
  assert(SDLogger_GetState() == SD_LOGGER_STOPPED);
  assert(strstr(sd_output, "safe to remove") != NULL);
  unsigned int writes = card_write_commands;
  SDLogger_Process(&sample, 11000U);
  assert(card_write_commands == writes);
  CheckFile("LOG00001.CSV",
      "1000,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15\r\n6000,,0,,,,,,,,0\r\n");
  SDLogger_Start();
  unsigned int initializations = card_init_commands;
  SDLogger_Process(&sample, UINT32_MAX - 1999U);
  assert(card_init_commands == initializations + 1U); /* Remount reinitializes physical card. */
  SDLogger_Process(&sample, 3000U); /* Tick rollover: exactly 5000 ms. */
  SDLogger_DisplayText(display, sizeof(display)); assert(strcmp(display, "SD LOG 2") == 0);
  SDLogger_Stop();
  CheckFile("LOG00002.CSV",
      "4294965296,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15\r\n"
      "3000,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15\r\n");
  /* Simulated reboot creates a fresh file, preserving earlier logs. */
  SDLogger_Init(&test_spi, GPIOA, GPIO_PIN_4);
  SDLogger_Process(&sample, 0U);
  assert(SDLogger_GetState() == SD_LOGGER_ACTIVE);
  assert(strstr(sd_output, "LOG00003.CSV") != NULL);
  card_write_reject = 1;
  SDLogger_Process(&sample, 5000U);
  assert(SDLogger_GetState() == SD_LOGGER_ERROR);
  SDLogger_DisplayText(display, sizeof(display));
  assert(strcmp(display, "SD WRITE REJECT C24") == 0);
  writes = card_write_commands;
  SDLogger_Process(&sample, 10000U);
  assert(card_write_commands == writes);
  sd_output[0] = '\0';
  SDLogger_Stop();
  assert(strstr(sd_output, "safe to remove") == NULL);
  card_write_reject = 0;
  SDLogger_Start(); SDLogger_Process(&invalid, 15000U);
  assert(SDLogger_GetState() == SD_LOGGER_ACTIVE);
  assert(get_fattime() == ((1UL << 21) | (1UL << 16)));
  SDLogger_Stop();
  CheckFile("LOG00004.CSV", "15000,,0,,,,,,,,0\r\n");
  CheckFile("LOG00001.CSV",
      "1000,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15\r\n6000,,0,,,,,,,,0\r\n");
  puts("FatFs logging: cadence, RTC validity, stop/restart, reboot, preserved files and write failures passed");
}

static void FullCardTest(void)
{
  FIL filler; UINT written;
  static char zeros[4096];
  SDLogger_Start(); SDLogger_Process(&sample, 0U);
  assert(SDLogger_GetState() == SD_LOGGER_ACTIVE);
  SDDisk_BeginOperation();
  assert(f_open(&filler, "FILL.BIN", FA_WRITE | FA_CREATE_NEW) == FR_OK);
  do {
    SDDisk_BeginOperation();
    assert(f_write(&filler, zeros, sizeof(zeros), &written) == FR_OK);
  } while (written == sizeof(zeros));
  SDDisk_BeginOperation(); assert(f_close(&filler) == FR_OK);
  sd_output[0] = '\0';
  /* Consume the remaining space in the logger's existing cluster, then force
     a partial record. The subsequent fsck checks its allocation was synced. */
  for (unsigned int i = 1U; i <= 100U && SDLogger_GetState() == SD_LOGGER_ACTIVE; ++i)
    SDLogger_Process(&sample, i * 5000U);
  assert(SDLogger_GetState() == SD_LOGGER_ERROR);
  assert(strstr(sd_output, "card full / short write") != NULL);
  char display[24]; SDLogger_DisplayText(display, sizeof(display));
  assert(strcmp(display, "SD FULL") == 0);
  puts("Full-card partial-row short-write detection passed");
}

int main(int argc, char **argv)
{
  assert(argc == 3);
  CardEmulator_Open(argv[1]);
  CSVTests();
  if (strcmp(argv[2], "blank") == 0)
  {
    SDLogger_Init(&test_spi, GPIOA, GPIO_PIN_4);
    unsigned int writes = card_write_commands;
    SDLogger_Process(&sample, 0U);
    assert(SDLogger_GetState() == SD_LOGGER_ERROR);
    assert(strstr(sd_output, "FAT16/FAT32") != NULL);
    char display[24]; SDLogger_DisplayText(display, sizeof(display));
    assert(strcmp(display, "SD FORMAT F13") == 0);
    assert(card_write_commands == writes); /* Never formats an unknown card. */
    puts("Unformatted card rejected without any write");
  }
  else
  {
    ProtocolTests();
    card_legacy = strcmp(argv[2], "fat16") == 0;
    LoggingTests();
    if (card_legacy) FullCardTest();
  }
  CardEmulator_Close();
  return 0;
}
