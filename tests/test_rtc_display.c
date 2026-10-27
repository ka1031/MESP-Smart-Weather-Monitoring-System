#include "ssd1306.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t a, uint32_t t, uint32_t ms)
{(void)h; (void)a; (void)t; (void)ms; return HAL_OK;}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a, uint8_t *d, uint16_t n, uint32_t ms)
{(void)h; (void)a; (void)d; (void)n; (void)ms; return HAL_OK;}
void HAL_Delay(uint32_t ms) {(void)ms;}
int main(void)
{
  SSD1306_Handle display = {0};
  uint8_t untouched[128];
  memset(untouched, 0xA5, sizeof(untouched));
  memcpy(&display.framebuffer[6U * 128U], untouched, sizeof(untouched));
  SSD1306_SetCursor(&display, 4U, 5U);
  SSD1306_WriteString(&display, "TIME 14:32:05");
  assert(display.cursor_page == 5U);
  /* Both colon separators must render, rather than turn into blank glyphs. */
  assert(display.framebuffer[5U * 128U + 4U + 7U * 6U + 1U] != 0U);
  assert(display.framebuffer[5U * 128U + 4U + 10U * 6U + 1U] != 0U);
  assert(memcmp(&display.framebuffer[6U * 128U], untouched, sizeof(untouched)) == 0);
  SSD1306_SetCursor(&display, 4U, 0U);
  SSD1306_WriteString(&display, "DATE 2026-10-24");
  assert(display.cursor_page == 0U);
  puts("RTC OLED layout and separators test passed");
  return 0;
}
