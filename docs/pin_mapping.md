# Pin assignments

The current CubeMX configuration also reserves PA4 (`SD_CS`, output initially
high), PA5 (SPI1 SCK), PA6 (SPI1 MISO, pull-up), and PA7 (SPI1 MOSI) for the
SD card interface. These assignments are saved in the `.ioc` and generated
by CubeMX. See [SD logging](sd_logging.md) for operation and wiring.

## Sensors and display

- PB8 / PB9: shared I2C1 SCL / SDA for BME280, BH1750, DS3231 and SSD1306.
- PA0: rain analog input (ADC1 channel 0).
- PA1: rain digital input, normally active-low.

## Serial interfaces

- PA2: USART2 TX → ESP32 GPIO16.
- PA3: USART2 RX ← ESP32 GPIO17, reserved return path.
- PA9 / PA10: USART1 TX / RX for the debug and RTC/SD command console.
- Both UARTs use 115200 baud, 8N1 and 3.3 V logic.

## Board functions

- PC13: active-low heartbeat LED.
- PA13 / PA14: SWDIO / SWCLK for programming and debugging.
- PD0 / PD1: external oscillator input / output.

All modules share ground. Weather-status LEDs and buzzer pins are not assigned yet.
