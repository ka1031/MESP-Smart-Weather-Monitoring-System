#ifndef SSD1306_H
#define SSD1306_H

#include "device_status.h"
#include "stm32f1xx_hal.h"

#include <stdint.h>

#define SSD1306_WIDTH        (128U)
#define SSD1306_HEIGHT       (64U)
#define SSD1306_BUFFER_SIZE  (SSD1306_WIDTH * SSD1306_HEIGHT / 8U)

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint8_t address_7bit;
  uint8_t initialized;
  uint8_t cursor_x;
  uint8_t cursor_page;
  uint8_t framebuffer[SSD1306_BUFFER_SIZE];
} SSD1306_Handle;

DeviceStatus SSD1306_Init(SSD1306_Handle *display,
                          I2C_HandleTypeDef *i2c,
                          uint8_t primary_address_7bit,
                          uint8_t secondary_address_7bit);
void SSD1306_Clear(SSD1306_Handle *display);
void SSD1306_DrawPixel(SSD1306_Handle *display,
                       uint8_t x,
                       uint8_t y,
                       uint8_t on);
void SSD1306_DrawRectangle(SSD1306_Handle *display,
                           uint8_t x,
                           uint8_t y,
                           uint8_t width,
                           uint8_t height);
void SSD1306_SetCursor(SSD1306_Handle *display, uint8_t x, uint8_t page);
void SSD1306_WriteString(SSD1306_Handle *display, const char *text);
DeviceStatus SSD1306_UpdateScreen(SSD1306_Handle *display);
const char *SSD1306_StatusString(DeviceStatus status);

#endif /* SSD1306_H */
