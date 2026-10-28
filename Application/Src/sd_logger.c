#include "sd_logger.h"
#include "sd_diskio.h"
#include "app_config.h"
#include "app_log.h"
#include "weather_csv.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static FATFS filesystem;
static FIL logfile;
static SDLogger_State state = SD_LOGGER_STOPPED;
static char filename[13];
static uint32_t rows, last_log_tick;
static DWORD file_time;
static FRESULT last_error;
static uint8_t full;
static char csv[256];

DWORD get_fattime(void) { return file_time; }

static void CacheTime(const WeatherData *data)
{
  /* 1980-01-01 is a filesystem placeholder, never an invented CSV reading. */
  file_time = (1UL << 21) | (1UL << 16);
  const WeatherTimestamp *t = &data->timestamp;
  if ((data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP) != 0U &&
      t->year >= 1980U && t->year <= 2107U)
    file_time = ((DWORD)(t->year - 1980U) << 25) | ((DWORD)t->month << 21) |
        ((DWORD)t->day << 16) | ((DWORD)t->hour << 11) |
        ((DWORD)t->minute << 5) | (t->second / 2U);
}

static const char *ErrorText(FRESULT error)
{
  switch (error)
  {
    case FR_NOT_READY: return "card not ready; check wiring/power/card";
    case FR_NO_FILESYSTEM: return "FAT16/FAT32 filesystem required";
    case FR_DISK_ERR: return "card I/O error; check wiring/power";
    case FR_WRITE_PROTECTED: return "write protected";
    case FR_DENIED: return "card/root directory full or names exhausted";
    default: return "filesystem error";
  }
}

void SDLogger_Report(void)
{
  char message[160];
  if (state == SD_LOGGER_ACTIVE)
    (void)snprintf(message, sizeof(message), "SD: LOGGING %s, %lu saved rows\r\n",
        filename, (unsigned long)rows);
  else if (state == SD_LOGGER_PENDING)
    (void)snprintf(message, sizeof(message), "SD: start requested; waiting for next sample\r\n");
  else if (state == SD_LOGGER_ERROR)
    (void)snprintf(message, sizeof(message), "SD: ERROR %u (%s). Fix card, then SD START.\r\n",
        (unsigned)last_error, full ? "card full / short write" : ErrorText(last_error));
  else
    (void)snprintf(message, sizeof(message), "SD: STOPPED; safe to remove card\r\n");
  (void)AppLog_Write(message);
  if (state == SD_LOGGER_ERROR && (last_error == FR_NOT_READY || last_error == FR_DISK_ERR))
    SDDisk_Report();
}

static void Fail(FRESULT error)
{
  last_error = error;
  state = SD_LOGGER_ERROR;
  /* Do not retry a damaged/removed medium or write cached sectors to a new card. */
  (void)f_mount(NULL, "", 0U);
  SDLogger_Report();
}

void SDLogger_Init(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin)
{
  SDDisk_Bind(spi, port, pin);
  state = SD_LOGGER_PENDING;
  filename[0] = '\0';
  rows = 0U;
  full = 0U;
  file_time = (1UL << 21) | (1UL << 16);
}

void SDLogger_Start(void)
{
  if (state != SD_LOGGER_ACTIVE)
  {
    (void)f_mount(NULL, "", 0U);
    state = SD_LOGGER_PENDING;
    full = 0U;
    rows = 0U;
    filename[0] = '\0';
  }
  SDLogger_Report();
}

void SDLogger_Stop(void)
{
  if (state == SD_LOGGER_ACTIVE)
  {
    SDDisk_BeginOperation();
    FRESULT result = f_close(&logfile);
    if (result != FR_OK) { Fail(result); return; }
  }
  /* An earlier I/O failure cannot be declared a successful clean close. */
  if (state == SD_LOGGER_ERROR) { SDLogger_Report(); return; }
  (void)f_mount(NULL, "", 0U);
  state = SD_LOGGER_STOPPED;
  SDLogger_Report();
}

static FRESULT WriteAndSync(const char *text, UINT length)
{
  UINT written = 0U;
  FRESULT result = f_write(&logfile, text, length, &written);
  if (result != FR_OK) return result;
  if (written != length)
  {
    /* Commit any partial final row and FAT allocation before latching FULL. */
    full = 1U;
    result = f_sync(&logfile);
    return result == FR_OK ? FR_DENIED : result;
  }
  return f_sync(&logfile);
}

static FRESULT OpenNewLog(void)
{
  DIR directory;
  FILINFO entry;
  uint32_t highest = 0U;
  FRESULT result = f_mount(&filesystem, "", 1U);
  if (result != FR_OK) return result;
  result = f_opendir(&directory, "/");
  if (result != FR_OK) return result;
  for (;;)
  {
    result = f_readdir(&directory, &entry);
    if (result != FR_OK || entry.fname[0] == '\0') break;
    if (strlen(entry.fname) == 12U && strncmp(entry.fname, "LOG", 3U) == 0 &&
        strcmp(entry.fname + 8U, ".CSV") == 0)
    {
      uint32_t number = 0U;
      uint8_t valid = 1U;
      for (unsigned int i = 3U; i < 8U; ++i)
      {
        if (entry.fname[i] < '0' || entry.fname[i] > '9') { valid = 0U; break; }
        number = number * 10U + (unsigned)(entry.fname[i] - '0');
      }
      if (valid && number > highest) highest = number;
    }
  }
  FRESULT close_result = f_closedir(&directory);
  if (result != FR_OK) return result;
  if (close_result != FR_OK) return close_result;
  /* CREATE_NEW is the final guard: existing logs are never truncated. */
  do {
    if (highest == 99999U) return FR_DENIED;
    ++highest;
    (void)snprintf(filename, sizeof(filename), "LOG%05lu.CSV", (unsigned long)(highest % 100000U));
    result = f_open(&logfile, filename, FA_WRITE | FA_CREATE_NEW);
  } while (result == FR_EXIST);
  if (result != FR_OK) return result;
  return WriteAndSync(WEATHER_CSV_HEADER, (UINT)strlen(WEATHER_CSV_HEADER));
}

void SDLogger_Process(const WeatherData *data, uint32_t sample_tick)
{
  if (data == NULL || state == SD_LOGGER_STOPPED || state == SD_LOGGER_ERROR) return;
  CacheTime(data);
  if (state == SD_LOGGER_PENDING)
  {
    SDDisk_BeginMount();
    FRESULT result = OpenNewLog();
    if (result != FR_OK) { Fail(result); return; }
    state = SD_LOGGER_ACTIVE;
    last_log_tick = sample_tick - APP_CONFIG_SD_LOG_PERIOD_MS;
    SDLogger_Report();
  }
  if ((uint32_t)(sample_tick - last_log_tick) < APP_CONFIG_SD_LOG_PERIOD_MS) return;
  last_log_tick = sample_tick;
  size_t length = WeatherCSV_Format(csv, sizeof(csv), data, sample_tick);
  if (length == 0U) { Fail(FR_INT_ERR); return; }
  /* Avoid FatFs' silent truncation at the FAT 4 GiB file-size limit. */
  if (f_size(&logfile) > UINT32_MAX - length) { full = 1U; Fail(FR_DENIED); return; }
  SDDisk_BeginOperation();
  FRESULT result = WriteAndSync(csv, (UINT)length);
  if (result != FR_OK) { Fail(result); return; }
  ++rows;
}

SDLogger_State SDLogger_GetState(void) { return state; }

void SDLogger_DisplayText(char *buffer, size_t capacity)
{
  if (state == SD_LOGGER_ACTIVE)
    (void)snprintf(buffer, capacity, "SD LOG %lu", (unsigned long)rows);
  else if (state == SD_LOGGER_ERROR)
  {
    if (last_error == FR_NOT_READY || last_error == FR_DISK_ERR)
      SDDisk_DiagnosticText(buffer, capacity);
    else if (full)
      (void)snprintf(buffer, capacity, "SD FULL");
    else if (last_error == FR_NO_FILESYSTEM)
      (void)snprintf(buffer, capacity, "SD FORMAT F%u", (unsigned)last_error);
    else
      (void)snprintf(buffer, capacity, "SD FILE F%u", (unsigned)last_error);
  }
  else
    (void)snprintf(buffer, capacity, "%s",
        state == SD_LOGGER_PENDING ? "SD STARTING" : "SD STOPPED");
}
