#include "rtc_console.h"
#include "app_log.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define RX_CAPACITY (128U)
#define LINE_CAPACITY (48U)

static UART_HandleTypeDef *console_uart;
static DS3231_Handle *console_rtc;
static uint8_t rx_byte;
static volatile uint8_t rx_buffer[RX_CAPACITY];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint8_t rx_fault;
static char line[LINE_CAPACITY];
static size_t line_length;
static uint8_t discard_line;

static uint16_t Number(const char *text, size_t digits)
{
  uint16_t value = 0U;
  for (size_t i = 0U; i < digits; ++i)
    value = (uint16_t)(value * 10U + (unsigned int)(text[i] - '0'));
  return value;
}

static void ReportTime(void)
{
  DS3231_DateTime dt;
  DeviceStatus status = DS3231_ReadDateTime(console_rtc, &dt);
  char message[80];
  if (status == DEVICE_STATUS_OK)
    (void)snprintf(message, sizeof(message),
        "RTC: %04u-%02u-%02u %02u:%02u:%02u\r\n",
        (unsigned int)dt.year, (unsigned int)dt.month, (unsigned int)dt.day,
        (unsigned int)dt.hour, (unsigned int)dt.minute, (unsigned int)dt.second);
  else
    (void)snprintf(message, sizeof(message), "RTC: %s\r\n", DS3231_StatusString(status));
  (void)AppLog_Write(message);
}

static void ExecuteLine(void)
{
  static const char format[] = "TIME SET 0000-00-00 00:00:00";
  static const uint8_t month_offsets[] = {0U, 3U, 2U, 5U, 0U, 3U,
                                         5U, 1U, 4U, 6U, 2U, 4U};
  DS3231_DateTime dt;
  uint16_t year;
  DeviceStatus status;
  if (strcmp(line, "TIME?") == 0)
  {
    ReportTime();
    return;
  }
  if (strcmp(line, "HELP") == 0)
  {
    (void)AppLog_Write("Commands: TIME? | TIME SET YYYY-MM-DD HH:MM:SS (2000-2099, 24h)\r\n");
    return;
  }
  if (strlen(line) != sizeof(format) - 1U) goto invalid;
  for (size_t i = 0U; i < sizeof(format) - 1U; ++i)
  {
    if (format[i] == '0')
    {
      if ((line[i] < '0') || (line[i] > '9')) goto invalid;
    }
    else if (line[i] != format[i]) goto invalid;
  }
  dt.year = Number(&line[9], 4U);
  dt.month = (uint8_t)Number(&line[14], 2U);
  dt.day = (uint8_t)Number(&line[17], 2U);
  dt.hour = (uint8_t)Number(&line[20], 2U);
  dt.minute = (uint8_t)Number(&line[23], 2U);
  dt.second = (uint8_t)Number(&line[26], 2U);
  dt.weekday = 1U;
  if (!DS3231_IsValidDateTime(&dt)) goto invalid;
  year = (uint16_t)(dt.year - ((dt.month < 3U) ? 1U : 0U));
  dt.weekday = (uint8_t)((year + year / 4U - year / 100U + year / 400U +
                          month_offsets[dt.month - 1U] + dt.day) % 7U + 1U);
  status = DS3231_SetDateTime(console_rtc, &dt);
  if (status == DEVICE_STATUS_OK)
  {
    (void)AppLog_Write("RTC time set.\r\n");
    ReportTime();
  }
  else
  {
    (void)AppLog_Write("RTC set failed: ");
    (void)AppLog_Write(DS3231_StatusString(status));
    (void)AppLog_Write("\r\n");
  }
  return;
invalid:
  (void)AppLog_Write("Invalid command/date. Use TIME SET YYYY-MM-DD HH:MM:SS, TIME?, or HELP.\r\n");
}

HAL_StatusTypeDef RTCConsole_Init(UART_HandleTypeDef *uart, DS3231_Handle *rtc)
{
  HAL_StatusTypeDef status;
  if ((uart == NULL) || (rtc == NULL) || (uart->Instance != USART1)) return HAL_ERROR;
  console_uart = uart;
  console_rtc = rtc;
  rx_head = rx_tail = 0U;
  rx_fault = 0U;
  line_length = 0U;
  discard_line = 0U;
  /* This application's USART1 receive IRQ is owned here, not by CubeMX. */
  HAL_NVIC_SetPriority(USART1_IRQn, 5U, 0U);
  __HAL_UART_CLEAR_OREFLAG(uart);
  status = HAL_UART_Receive_IT(uart, &rx_byte, 1U);
  if (status == HAL_OK) HAL_NVIC_EnableIRQ(USART1_IRQn);
  return status;
}

void RTCConsole_IRQHandler(void)
{
  if (console_uart != NULL) HAL_UART_IRQHandler(console_uart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  uint16_t next;
  if (uart != console_uart) return;
  next = (uint16_t)((rx_head + 1U) % RX_CAPACITY);
  if ((next == rx_tail) || (uart->ErrorCode != HAL_UART_ERROR_NONE)) rx_fault = 1U;
  else
  {
    rx_buffer[rx_head] = rx_byte;
    rx_head = next;
  }
  if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK) rx_fault = 1U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart == console_uart) rx_fault = 1U;
}

void RTCConsole_Poll(void)
{
  if (console_uart == NULL) return;
  /* Bound each pass so continuous serial traffic cannot starve sampling. */
  for (size_t i = 0U; i < RX_CAPACITY; ++i)
  {
    uint8_t byte;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (rx_fault != 0U)
    {
      rx_tail = rx_head;
      rx_fault = 0U;
      (void)HAL_UART_AbortReceive(console_uart);
      __HAL_UART_CLEAR_OREFLAG(console_uart);
      if (HAL_UART_Receive_IT(console_uart, &rx_byte, 1U) != HAL_OK) rx_fault = 1U;
      __set_PRIMASK(primask);
      line_length = 0U;
      discard_line = 1U;
      (void)AppLog_Write("Serial input lost. Press Enter, then resend the command.\r\n");
      return;
    }
    if (rx_tail == rx_head)
    {
      __set_PRIMASK(primask);
      return;
    }
    byte = rx_buffer[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % RX_CAPACITY);
    __set_PRIMASK(primask);
    if ((byte == '\r') || (byte == '\n'))
    {
      if (discard_line != 0U)
        (void)AppLog_Write("Command discarded. Please resend.\r\n");
      else if (line_length != 0U)
      {
        line[line_length] = '\0';
        ExecuteLine();
      }
      line_length = 0U;
      discard_line = 0U;
    }
    else if (discard_line == 0U)
    {
      if ((byte == 8U) || (byte == 127U))
      {
        if (line_length > 0U) --line_length;
      }
      else if ((byte < 32U) || (byte > 126U) || (line_length >= sizeof(line) - 1U))
        discard_line = 1U;
      else line[line_length++] = (char)byte;
    }
  }
}
