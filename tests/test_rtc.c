#include "ds3231.h"
#include "rtc_console.h"
#include "app_log.h"
#include "weather_app.h"
#include "bme280.h"
#include "bh1750.h"
#include "rain_sensor.h"
#include "ssd1306.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t registers[0x13];
static I2C_HandleTypeDef bus;
static UART_HandleTypeDef uart = {USART1, 0U};
static DS3231_Handle rtc;
static HAL_StatusTypeDef ready_status;
static HAL_StatusTypeDef read_status;
static int fail_write_reg = -1;
static unsigned int write_count;
static uint8_t *receive_target;
static char output[8192];
static int stop_during_read;

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr,
                                        uint32_t tries, uint32_t timeout)
{
  assert(h == &bus && addr == 0xD0U && tries > 0U && timeout > 0U);
  return ready_status;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t addr,
    uint16_t reg, uint16_t reg_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
  assert(h == &bus && addr == 0xD0U && reg_size == 1U && timeout > 0U);
  assert(reg + size <= sizeof(registers));
  if (read_status != HAL_OK) return read_status;
  if (reg == 0U) assert(size == 7U); /* Must latch a complete time snapshot. */
  memcpy(data, &registers[reg], size);
  if (stop_during_read && reg == 0U) registers[0x0F] |= 0x80U;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t addr,
    uint16_t reg, uint16_t reg_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
  assert(h == &bus && addr == 0xD0U && reg_size == 1U && timeout > 0U);
  ++write_count;
  if ((int)reg == fail_write_reg) return HAL_TIMEOUT;
  if (reg == 0x0FU)
  {
    /* Alarm flags can only be cleared by writing zero; ones preserve them. */
    registers[reg] = (uint8_t)((data[0] & 0x88U) | (registers[reg] & data[0] & 3U));
  }
  else memcpy(&registers[reg], data, size);
  return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{assert(h == &uart && n == 1U); receive_target = p; return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h)
{assert(h == &uart); return HAL_OK;}
void HAL_UART_IRQHandler(UART_HandleTypeDef *h) {(void)h;}
void HAL_NVIC_SetPriority(int irq, uint32_t p, uint32_t sub)
{assert(irq == USART1_IRQn); (void)p; (void)sub;}
void HAL_NVIC_EnableIRQ(int irq) {assert(irq == USART1_IRQn);}
HAL_StatusTypeDef AppLog_Write(const char *s)
{assert(strlen(output) + strlen(s) < sizeof(output)); strcat(output, s); return HAL_OK;}

static void Reset(void)
{
  memset(registers, 0, sizeof(registers));
  ready_status = read_status = HAL_OK;
  fail_write_reg = -1;
  stop_during_read = 0;
  write_count = 0;
  rtc = (DS3231_Handle){0};
  output[0] = '\0';
  uart.ErrorCode = 0U;
  assert(DS3231_Init(&rtc, &bus, 0x68U) == DEVICE_STATUS_OK);
}
static void Send(const char *s)
{
  while (*s) {*receive_target = (uint8_t)*s++; HAL_UART_RxCpltCallback(&uart);}
}
static void Command(const char *s) {Send(s); RTCConsole_Poll();}
static DS3231_DateTime Date(void)
{return (DS3231_DateTime){2026U, 10U, 24U, 7U, 14U, 32U, 5U};}

static void TestCalendar(void)
{
  DS3231_DateTime dt = Date(), out;
  Reset();
  assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_OK);
  assert(registers[0] == 0x05 && registers[1] == 0x32 && registers[2] == 0x14);
  assert(registers[4] == 0x24 && registers[5] == 0x10 && registers[6] == 0x26);
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_OK);
  assert(out.year == 2026 && out.month == 10 && out.day == 24 && out.weekday == 7);
  assert(out.hour == 14 && out.minute == 32 && out.second == 5);
  dt.year = 2000; dt.month = 2; dt.day = 29;
  assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_OK);
  dt.year = 2024; assert(DS3231_IsValidDateTime(&dt));
  dt.year = 2025; assert(!DS3231_IsValidDateTime(&dt));
  dt.year = 2100; assert(!DS3231_IsValidDateTime(&dt));
  dt = Date(); dt.month = 4; dt.day = 31; assert(!DS3231_IsValidDateTime(&dt));
  dt = Date(); dt.hour = 24; assert(!DS3231_IsValidDateTime(&dt));
  dt = Date(); dt.minute = 60; assert(!DS3231_IsValidDateTime(&dt));
  dt = Date(); dt.second = 60; assert(!DS3231_IsValidDateTime(&dt));
  dt = Date(); dt.weekday = 0; assert(!DS3231_IsValidDateTime(&dt));
  write_count = 0;
  assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_INVALID_ARGUMENT);
  assert(write_count == 0);
}
static void TestReadFormats(void)
{
  DS3231_DateTime dt = Date(), out;
  Reset(); assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_OK);
  registers[2] = 0x52; /* 12 AM */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_OK && out.hour == 0);
  registers[2] = 0x72; /* 12 PM */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_OK && out.hour == 12);
  registers[2] = 0x61; /* 1 PM */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_OK && out.hour == 13);
  registers[2] = 0x40; /* invalid 0 AM */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_DATA_ERROR);
  registers[2] = 0x23; registers[0] = 0x5A; /* illegal BCD digit */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_DATA_ERROR);
  registers[0] = 0x59; registers[5] |= 0x80; /* unsupported century */
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_DATA_ERROR);
  registers[5] = 0x02; registers[4] = 0x30;
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_DATA_ERROR);
}
static void TestPowerAndErrors(void)
{
  DS3231_DateTime dt = Date(), out = {0};
  Reset(); registers[0x0E] = 0x9FU; registers[0x0F] = 0x8BU;
  assert(DS3231_Init(&rtc, &bus, 0x68) == DEVICE_STATUS_OK);
  assert(registers[0x0E] == 0x1F && registers[0x0F] == 0x8B);
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_TIME_NOT_SET && out.year == 0);
  assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_OK);
  assert(registers[0x0F] == 0x0B); /* OSF cleared; output and alarms preserved. */
  write_count = 0;
  assert(DS3231_Init(&rtc, &bus, 0x68) == DEVICE_STATUS_OK && write_count == 0);
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_OK && out.year == 2026);
  read_status = HAL_TIMEOUT;
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_TIMEOUT && out.year == 2026);
  read_status = HAL_OK; stop_during_read = 1;
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_TIME_NOT_SET);
  stop_during_read = 0; registers[0x0F] = 0U; fail_write_reg = 0;
  assert(DS3231_SetDateTime(&rtc, &dt) == DEVICE_STATUS_TIMEOUT);
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_TIME_NOT_SET);
  ready_status = HAL_ERROR;
  assert(DS3231_Init(&rtc, &bus, 0x68) == DEVICE_STATUS_NOT_FOUND && !rtc.initialized);
  assert(DS3231_ReadDateTime(&rtc, &out) == DEVICE_STATUS_NOT_INITIALIZED);
  assert(DS3231_Init(NULL, &bus, 0x68) == DEVICE_STATUS_INVALID_ARGUMENT);
  assert(DS3231_Init(&rtc, &bus, 0x69) == DEVICE_STATUS_INVALID_ARGUMENT);
}
static void TestConsole(void)
{
  unsigned int before;
  Reset(); registers[0x0F] = 0x80;
  assert(RTCConsole_Init(&uart, &rtc) == HAL_OK);
  Command("TIME?\r\n"); assert(strstr(output, "RTC time not set"));
  output[0] = '\0';
  /* Entire pasted command arrives while the main loop is busy. */
  Command("TIME SET 2026-10-24 14:32:05\r\n");
  assert(strstr(output, "RTC time set.") && strstr(output, "2026-10-24 14:32:05"));
  assert(registers[3] == 7U);
  before = write_count;
  Command("TIME SET 2025-02-29 12:00:00\n");
  Command("TIME SET 2026-10-24 24:00:00\n");
  Command("TIME SET 2026-10-24 14:32:05junk\n");
  Command("TIME SET 2026/10/24 14:32:05\n");
  Command("TIME SET 2100-01-01 00:00:00\n");
  assert(write_count == before);
  Command("TIME SET 2024-02-29 00:00:00\n");
  assert(registers[3] == 5U); /* Thursday */
  before = write_count;
  for (unsigned int i = 0; i < 60U; ++i) Command("X");
  Command("TIME SET 2026-10-24 14:32:05\n");
  assert(write_count == before); /* Overlong line cannot execute a suffix. */
  for (unsigned int i = 0; i < 140U; ++i) Send("X");
  RTCConsole_Poll();
  assert(strstr(output, "Serial input lost"));
  Command("\nTIME SET 2026-10-24 14:32:05\n");
  assert(write_count > before);
  before = write_count;
  Send("TIME SET 2026-10-"); uart.ErrorCode = 8U; HAL_UART_ErrorCallback(&uart);
  RTCConsole_Poll(); uart.ErrorCode = 0U;
  Command("24 14:32:05\n"); assert(write_count == before);
  Command("TIME SET 2026-10-24 14:32:05\n"); assert(write_count > before);
  before = write_count;
  Command("TIME SET 2026-10-24 14:32:06\b5\n");
  assert(write_count > before && registers[0] == 5U);
  puts("RTC driver and serial command tests passed");
}
static uint32_t tick;
static unsigned int sensor_reads;
static char display_lines[8][24];
static uint8_t display_page;
uint32_t HAL_GetTick(void) {return tick;}
void HAL_GPIO_TogglePin(GPIO_TypeDef *gpio, uint16_t pin) {(void)gpio; (void)pin;}
void AppLog_Init(UART_HandleTypeDef *h) {assert(h == &uart);}
DeviceStatus BME280_Init(BME280_Handle *d, I2C_HandleTypeDef *h, uint8_t addr)
{d->i2c = h; d->address_7bit = addr; return DEVICE_STATUS_OK;}
DeviceStatus BME280_Read(BME280_Handle *d, BME280_Measurement *m)
{(void)d; ++sensor_reads; *m = (BME280_Measurement){28.4F, 64.0F, 1008.0F}; return DEVICE_STATUS_OK;}
const char *BME280_StatusString(DeviceStatus s) {(void)s; return "test";}
DeviceStatus BH1750_Init(BH1750_Handle *d, I2C_HandleTypeDef *h, uint8_t addr)
{d->i2c = h; d->address_7bit = addr; return DEVICE_STATUS_OK;}
DeviceStatus BH1750_ReadLux(BH1750_Handle *d, float *lux)
{(void)d; ++sensor_reads; *lux = 485.0F; return DEVICE_STATUS_OK;}
const char *BH1750_StatusString(DeviceStatus s) {(void)s; return "test";}
DeviceStatus RainSensor_Init(RainSensor_Handle *d, ADC_HandleTypeDef *a, const RainSensor_Config *c)
{(void)d; (void)a; (void)c; return DEVICE_STATUS_OK;}
DeviceStatus RainSensor_Read(RainSensor_Handle *d, RainSensor_Measurement *m)
{(void)d; ++sensor_reads; *m = (RainSensor_Measurement){3000U, 25U, 0U}; return DEVICE_STATUS_OK;}
const char *RainSensor_StatusString(DeviceStatus s) {(void)s; return "test";}
DeviceStatus SSD1306_Init(SSD1306_Handle *d, I2C_HandleTypeDef *h, uint8_t a, uint8_t b)
{(void)h; (void)b; d->address_7bit = a; d->initialized = 1U; return DEVICE_STATUS_OK;}
void SSD1306_Clear(SSD1306_Handle *d) {(void)d; memset(display_lines, 0, sizeof(display_lines));}
void SSD1306_SetCursor(SSD1306_Handle *d, uint8_t x, uint8_t page)
{(void)d; (void)x; assert(page < 8U); display_page = page;}
void SSD1306_WriteString(SSD1306_Handle *d, const char *text)
{(void)d; assert(strlen(text) <= 20U); strcpy(display_lines[display_page], text);}
DeviceStatus SSD1306_UpdateScreen(SSD1306_Handle *d) {(void)d; return DEVICE_STATUS_OK;}
const char *SSD1306_StatusString(DeviceStatus s) {(void)s; return "test";}

static void TestApplication(void)
{
  ADC_HandleTypeDef adc;
  const WeatherData *data;
  Reset(); tick = 0; sensor_reads = 0; registers[0x0F] = 0x80;
  WeatherApp_Init(&bus, &adc, &uart);
  WeatherApp_Run(); data = WeatherApp_GetLatestData();
  assert(sensor_reads == 3U && (data->valid_flags & 7U) == 7U);
  assert(!(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP));
  assert(data->timestamp.year == 0 && strstr(output, "timestamp INVALID"));
  assert(strcmp(display_lines[5], "RTC TIME NOT SET") == 0);
  Send("TIME SET 2026-10-24 14:32:05\n");
  WeatherApp_Run(); /* Commands work even before the next sample is due. */
  assert(sensor_reads == 3U && strstr(output, "RTC time set."));
  tick += 1000U; WeatherApp_Run();
  assert((data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP) && data->timestamp.year == 2026);
  assert(strcmp(display_lines[0], "DATE 2026-10-24") == 0);
  assert(strcmp(display_lines[5], "TIME 14:32:05") == 0);
  assert(strstr(display_lines[1], "TEMP") && strstr(display_lines[6], "RAIN"));
  read_status = HAL_TIMEOUT; tick += 1000U; WeatherApp_Run();
  assert(!(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP) && data->timestamp.year == 0);
  assert(sensor_reads == 9U && strcmp(display_lines[5], "RTC ERROR") == 0);
  read_status = HAL_OK; tick += 1000U; WeatherApp_Run();
  assert(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP);
  registers[0x0F] |= 0x80U; tick += 1000U; WeatherApp_Run();
  assert(!(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP));
  ready_status = HAL_ERROR; WeatherApp_Init(&bus, &adc, &uart); WeatherApp_Run();
  assert(!(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP));
  ready_status = HAL_OK; tick += 1000U; WeatherApp_Run();
  Send("TIME SET 2026-10-24 14:32:05\n");
  tick += 1000U; WeatherApp_Run();
  assert(data->valid_flags & WEATHER_DATA_VALID_TIMESTAMP);
  puts("Weather application RTC/display/fault recovery tests passed");
}

int main(void)
{
  TestCalendar(); TestReadFormats(); TestPowerAndErrors(); TestConsole(); TestApplication();
  return 0;
}
