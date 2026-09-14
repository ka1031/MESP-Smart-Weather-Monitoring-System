#include "ssd1306.h"

#include <stddef.h>
#include <string.h>

#define SSD1306_CONTROL_COMMAND       (0x00U)
#define SSD1306_CONTROL_DATA          (0x40U)
#define SSD1306_I2C_TIMEOUT_MS        (100U)
#define SSD1306_READY_TRIALS          (3U)
#define SSD1306_DATA_CHUNK_SIZE       (16U)

/* Space, digits, and uppercase letters in a compact 5x7 font. */
static const uint8_t ssd1306_font_5x7[][5] = {
  {0x00,0x00,0x00,0x00,0x00},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static DeviceStatus SSD1306_FromHalStatus(HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return DEVICE_STATUS_OK;
  }
  if (status == HAL_TIMEOUT)
  {
    return DEVICE_STATUS_TIMEOUT;
  }
  return DEVICE_STATUS_IO_ERROR;
}

static DeviceStatus SSD1306_WriteCommand(SSD1306_Handle *display,
                                         uint8_t command)
{
  uint8_t packet[2] = {SSD1306_CONTROL_COMMAND, command};

  return SSD1306_FromHalStatus(HAL_I2C_Master_Transmit(
      display->i2c,
      (uint16_t)((uint16_t)display->address_7bit << 1),
      packet,
      sizeof(packet),
      SSD1306_I2C_TIMEOUT_MS));
}

static const uint8_t *SSD1306_GetGlyph(char character)
{
  static const uint8_t glyph_dot[5] = {0x00,0x60,0x60,0x00,0x00};
  static const uint8_t glyph_minus[5] = {0x08,0x08,0x08,0x08,0x08};

  if (character == '.')
  {
    return glyph_dot;
  }
  if (character == '-')
  {
    return glyph_minus;
  }
  if (character == ' ')
  {
    return ssd1306_font_5x7[0];
  }
  if ((character >= '0') && (character <= '9'))
  {
    return ssd1306_font_5x7[1U + (uint8_t)(character - '0')];
  }
  if ((character >= 'a') && (character <= 'z'))
  {
    character = (char)(character - ('a' - 'A'));
  }
  if ((character >= 'A') && (character <= 'Z'))
  {
    return ssd1306_font_5x7[11U + (uint8_t)(character - 'A')];
  }
  return ssd1306_font_5x7[0];
}

DeviceStatus SSD1306_Init(SSD1306_Handle *display,
                          I2C_HandleTypeDef *i2c,
                          uint8_t primary_address_7bit,
                          uint8_t secondary_address_7bit)
{
  static const uint8_t init_commands[] = {
    0xAEU, 0x20U, 0x00U, 0xB0U, 0xC8U, 0x00U, 0x10U, 0x40U,
    0x81U, 0x7FU, 0xA1U, 0xA6U, 0xA8U, 0x3FU, 0xA4U, 0xD3U,
    0x00U, 0xD5U, 0x80U, 0xD9U, 0xF1U, 0xDAU, 0x12U, 0xDBU,
    0x40U, 0x8DU, 0x14U, 0xAFU
  };
  HAL_StatusTypeDef primary_status;
  HAL_StatusTypeDef secondary_status;
  DeviceStatus status;
  size_t command_index;

  if ((display == NULL) || (i2c == NULL) ||
      (primary_address_7bit > 0x7FU) ||
      (secondary_address_7bit > 0x7FU))
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }

  display->i2c = i2c;
  display->address_7bit = primary_address_7bit;
  display->initialized = 0U;

  primary_status = HAL_I2C_IsDeviceReady(
      i2c,
      (uint16_t)((uint16_t)primary_address_7bit << 1),
      SSD1306_READY_TRIALS,
      SSD1306_I2C_TIMEOUT_MS);
  if (primary_status != HAL_OK)
  {
    display->address_7bit = secondary_address_7bit;
    secondary_status = HAL_I2C_IsDeviceReady(
        i2c,
        (uint16_t)((uint16_t)secondary_address_7bit << 1),
        SSD1306_READY_TRIALS,
        SSD1306_I2C_TIMEOUT_MS);
    if (secondary_status != HAL_OK)
    {
      if ((primary_status == HAL_TIMEOUT) || (secondary_status == HAL_TIMEOUT))
      {
        return DEVICE_STATUS_TIMEOUT;
      }
      return DEVICE_STATUS_NOT_FOUND;
    }
  }

  HAL_Delay(100U);
  for (command_index = 0U;
       command_index < sizeof(init_commands);
       ++command_index)
  {
    status = SSD1306_WriteCommand(display, init_commands[command_index]);
    if (status != DEVICE_STATUS_OK)
    {
      return status;
    }
  }

  display->initialized = 1U;
  SSD1306_Clear(display);
  status = SSD1306_UpdateScreen(display);
  if (status != DEVICE_STATUS_OK)
  {
    display->initialized = 0U;
  }
  return status;
}

void SSD1306_Clear(SSD1306_Handle *display)
{
  if (display == NULL)
  {
    return;
  }

  memset(display->framebuffer, 0, sizeof(display->framebuffer));
  display->cursor_x = 0U;
  display->cursor_page = 0U;
}

void SSD1306_DrawPixel(SSD1306_Handle *display,
                       uint8_t x,
                       uint8_t y,
                       uint8_t on)
{
  uint16_t buffer_index;
  uint8_t bit_mask;

  if ((display == NULL) || (x >= SSD1306_WIDTH) || (y >= SSD1306_HEIGHT))
  {
    return;
  }

  buffer_index = (uint16_t)x + ((uint16_t)(y / 8U) * SSD1306_WIDTH);
  bit_mask = (uint8_t)(1U << (y % 8U));
  if (on != 0U)
  {
    display->framebuffer[buffer_index] |= bit_mask;
  }
  else
  {
    display->framebuffer[buffer_index] &= (uint8_t)~bit_mask;
  }
}

void SSD1306_DrawRectangle(SSD1306_Handle *display,
                           uint8_t x,
                           uint8_t y,
                           uint8_t width,
                           uint8_t height)
{
  uint16_t position;

  if ((display == NULL) || (width == 0U) || (height == 0U))
  {
    return;
  }

  for (position = x; position < ((uint16_t)x + width); ++position)
  {
    SSD1306_DrawPixel(display, (uint8_t)position, y, 1U);
    SSD1306_DrawPixel(display,
                      (uint8_t)position,
                      (uint8_t)(y + height - 1U),
                      1U);
  }
  for (position = y; position < ((uint16_t)y + height); ++position)
  {
    SSD1306_DrawPixel(display, x, (uint8_t)position, 1U);
    SSD1306_DrawPixel(display,
                      (uint8_t)(x + width - 1U),
                      (uint8_t)position,
                      1U);
  }
}

void SSD1306_SetCursor(SSD1306_Handle *display, uint8_t x, uint8_t page)
{
  if (display == NULL)
  {
    return;
  }

  display->cursor_x = x;
  display->cursor_page = page;
}

void SSD1306_WriteString(SSD1306_Handle *display, const char *text)
{
  const uint8_t *glyph;
  uint8_t column;

  if ((display == NULL) || (text == NULL))
  {
    return;
  }

  while ((*text != '\0') &&
         (display->cursor_page < (SSD1306_HEIGHT / 8U)))
  {
    glyph = SSD1306_GetGlyph(*text++);
    if (((uint16_t)display->cursor_x + 5U) >= SSD1306_WIDTH)
    {
      display->cursor_x = 0U;
      ++display->cursor_page;
      if (display->cursor_page >= (SSD1306_HEIGHT / 8U))
      {
        break;
      }
    }

    for (column = 0U; column < 5U; ++column)
    {
      display->framebuffer[((uint16_t)display->cursor_page * SSD1306_WIDTH) +
                           display->cursor_x] = glyph[column];
      ++display->cursor_x;
    }
    display->framebuffer[((uint16_t)display->cursor_page * SSD1306_WIDTH) +
                         display->cursor_x] = 0U;
    ++display->cursor_x;
  }
}

DeviceStatus SSD1306_UpdateScreen(SSD1306_Handle *display)
{
  uint8_t packet[SSD1306_DATA_CHUNK_SIZE + 1U];
  uint8_t page;
  uint8_t x;
  DeviceStatus status;

  if (display == NULL)
  {
    return DEVICE_STATUS_INVALID_ARGUMENT;
  }
  if (display->initialized == 0U)
  {
    return DEVICE_STATUS_NOT_INITIALIZED;
  }

  packet[0] = SSD1306_CONTROL_DATA;
  for (page = 0U; page < (SSD1306_HEIGHT / 8U); ++page)
  {
    status = SSD1306_WriteCommand(display, (uint8_t)(0xB0U + page));
    if (status != DEVICE_STATUS_OK)
    {
      return status;
    }
    status = SSD1306_WriteCommand(display, 0x00U);
    if (status != DEVICE_STATUS_OK)
    {
      return status;
    }
    status = SSD1306_WriteCommand(display, 0x10U);
    if (status != DEVICE_STATUS_OK)
    {
      return status;
    }

    for (x = 0U; x < SSD1306_WIDTH; x += SSD1306_DATA_CHUNK_SIZE)
    {
      memcpy(&packet[1],
             &display->framebuffer[((uint16_t)page * SSD1306_WIDTH) + x],
             SSD1306_DATA_CHUNK_SIZE);
      status = SSD1306_FromHalStatus(HAL_I2C_Master_Transmit(
          display->i2c,
          (uint16_t)((uint16_t)display->address_7bit << 1),
          packet,
          sizeof(packet),
          SSD1306_I2C_TIMEOUT_MS));
      if (status != DEVICE_STATUS_OK)
      {
        return status;
      }
    }
  }

  return DEVICE_STATUS_OK;
}

const char *SSD1306_StatusString(DeviceStatus status)
{
  switch (status)
  {
    case DEVICE_STATUS_OK:
      return "ok";
    case DEVICE_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case DEVICE_STATUS_NOT_INITIALIZED:
      return "not initialized";
    case DEVICE_STATUS_NOT_FOUND:
      return "display not found";
    case DEVICE_STATUS_IO_ERROR:
      return "I2C error";
    case DEVICE_STATUS_TIMEOUT:
      return "timeout";
    default:
      return "unknown error";
  }
}
