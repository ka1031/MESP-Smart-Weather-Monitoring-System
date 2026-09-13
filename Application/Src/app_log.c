#include "app_log.h"

#include "app_config.h"

#include <stddef.h>
#include <string.h>

static UART_HandleTypeDef *debug_uart_handle;

void AppLog_Init(UART_HandleTypeDef *uart)
{
  debug_uart_handle = uart;
}

HAL_StatusTypeDef AppLog_Write(const char *message)
{
  size_t length;

  if ((debug_uart_handle == NULL) || (message == NULL))
  {
    return HAL_ERROR;
  }

  length = strlen(message);
  if (length > UINT16_MAX)
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(debug_uart_handle,
                           (uint8_t *)message,
                           (uint16_t)length,
                           APP_CONFIG_UART_TIMEOUT_MS);
}
