# MESP Smart Weather Monitoring System

A modular weather-monitoring prototype based on the STM32F103C8T6 and STM32
HAL. The project intentionally uses a simple polling-based superloop; it does
not currently require an RTOS or an additional application framework.

## Current prototype

- BME280 temperature, humidity, and pressure readings
- BH1750 ambient-light readings
- Rain-module analog averaging and active-low digital wet/dry detection
- DS3231 date/time, power-loss detection, and serial clock setting
- SSD1306 128x64 OLED output with date and time
- Matching diagnostics through USART1 at 115200 baud
- SD CSV logging every five seconds, with RTC timestamps and safe stop/start commands
- Versioned, checksummed sensor packets on USART2 for an ESP32 receiver
- PC13 heartbeat toggled after every one-second sample

ESP32 receiver firmware and a local Wi-Fi readings page are provided in `ESP32/`.
Cloud connectivity and weather classification remain future work.
Local SD logging and the STM32 UART sender are implemented. Wind sensing has
been removed from scope; the SSD1306 OLED replaces the proposed 20×4 LCD.

## Architecture

The source tree separates generated platform code, hardware drivers, and
application behavior:

```text
Application/
  Inc/                  Application APIs, data types, and configuration
  Src/                  Weather orchestration and diagnostic logging
Core/
  Inc/                  STM32CubeMX-generated headers
  Src/                  STM32CubeMX-generated startup and peripheral setup
Drivers/
  CMSIS/                ST CMSIS package
  STM32F1xx_HAL_Driver/ ST HAL package
  Devices/
    Inc/                Project-owned device driver interfaces
    Src/                Project-owned device driver implementations
cmake/                  STM32CubeMX CMake integration
docs/                   Pin mapping and hardware documentation
modules/                Module roadmap and test notes
```

### Ownership rules

- `MESP_WeatherStation.ioc`, `Core`, and `cmake/stm32cubemx` are owned by
  STM32CubeMX.
- Custom changes inside generated C files must stay between matching
  `USER CODE BEGIN` and `USER CODE END` markers.
- Project drivers must use STM32 HAL and must not access application globals.
  HAL handles and device addresses are supplied through each driver's API.
- `Application/Src/weather_app.c` owns device coordination, sampling order,
  validity tracking, and presentation of collected data.
- `Application/Inc/app_config.h` is the home for I2C addresses, timeouts,
  sampling intervals, thresholds, and feature-selection constants.
- MCU pin definitions remain in the CubeMX-generated `Core/Inc/main.h`.

The BME280 driver uses forced-mode I2C measurements and Bosch's integer
compensation formulas. The BH1750 driver uses one-time high-resolution
measurements. The rain driver calibrates ADC1 once, averages 16 samples, and
reads the module's digital comparator output. The OLED driver probes addresses
0x3C and 0x3D and maintains its own framebuffer. No driver returns simulated
sensor readings.

## Current CubeMX configuration

- STM32F103C8T6 at 72 MHz from HSE and PLL
- I2C1 at 100 kHz, remapped to PB8/PB9
- ADC1 channel 0 on PA0 at a 12 MHz ADC clock
- Rain digital input on PA1
- Status LED output on PC13
- USART1 at 115200 baud on PA9/PA10
- USART2 at 115200 baud on PA2/PA3
- SWD on PA13/PA14
- SPI1 SD card: PA4 CS, PA5 SCK, PA6 MISO, PA7 MOSI; mode 0 at 281.25 kHz

SD logging creates a new numbered CSV file at boot and saves a row every five
seconds. The OLED shows `SD LOG <rows>`; USART1 supports `SD?`, `SD START`, and
`SD STOP`. Stop and wait for the clean-close confirmation before removal.
See [SD logging instructions](docs/sd_logging.md). The [read-only diagnostic](docs/sd_diagnostic.md)
remains available as a separate build-time mode.

The STM32 sends a `$MESP,1,...*CRC16` packet over USART2 after each sensor sample,
normally once per second. Connect PA2 to ESP32 GPIO16 and share ground. PA3 to
GPIO17 is reserved for future return traffic. USART1 retains its debug console.
See [STM32–ESP32 protocol](docs/stm32_esp32_protocol.md) for fields, checksum,
wiring, and receiver requirements. See [ESP32 upload instructions](ESP32/UPLOAD_INSTRUCTIONS.md)
for the separate ESP32 sketch, wiring and local Wi-Fi page.

Peripheral assignments are controlled by `MESP_WeatherStation.ioc`. External
I2C devices such as the DS3231 do not require the STM32 internal RTC peripheral.

### Temporary I2C scanner

`Application/Src/temporary_i2c_scanner.c` remains available as a temporary
hardware-debug tool. It is disabled by default. Set
`APP_CONFIG_ENABLE_I2C_SCANNER` to `1U` in `Application/Inc/app_config.h` to run
one startup scan and print responding addresses through USART1.

## Prototype wiring

- BME280, BH1750, SSD1306, and DS3231: SCL to PB8 and SDA to PB9
- Rain module AO: PA0
- Rain module DO: PA1
- All module grounds: common with the STM32 ground
- Power the I2C modules from 3.3 V unless the exact breakout board is known to
  include safe level shifting

The shared I2C bus requires pull-up resistors to 3.3 V. Many breakout boards
already provide pull-ups; avoid adding unnecessarily strong parallel pull-ups.

The displayed rain percentage is an estimate until the sensor is calibrated.
Measure the raw value with a completely dry plate and a repeatable wet plate,
then replace `APP_CONFIG_RAIN_DRY_RAW` and `APP_CONFIG_RAIN_WET_RAW` in
`Application/Inc/app_config.h`. Wet/dry text comes from the module's adjustable
digital comparator and may be tuned with its potentiometer.

## Building

Install the Arm GNU embedded toolchain, CMake 3.22 or newer, and Ninja. Ensure
`arm-none-eabi-gcc`, `cmake`, and `ninja` are available on the command line.

Configure and build the Debug version from the repository root:

```sh
cmake --preset Debug
cmake --build --preset Debug
```

The Debug build applies `-Wall`, `-Wextra`, and `-Wpedantic`. The resulting
firmware image is generated under `build/Debug`. A release build can be
produced with:

```sh
cmake --preset Release
cmake --build --preset Release
```

Flash the STM32 ELF using ST-Link/OpenOCD or STM32CubeProgrammer.
Build and upload the ESP32 separately through PlatformIO; see the
[ESP32 setup guide](ESP32/UPLOAD_INSTRUCTIONS.md).

## Adding a device implementation

Implement and test one driver at a time under `Drivers/Devices`. After a driver
works independently, create its handle in `weather_app.c`, initialize it from
`WeatherApp_Init`, and schedule non-blocking or short polling operations from
`WeatherApp_Run`. Keep device-specific registers and conversion formulas out of
application code.

## DS3231 clock and serial commands

Connect the DS3231 VCC to 3.3 V, GND to common ground, SCL to PB8, and SDA
to PB9. SQW/INT and 32K are unused. The existing 100 kHz I2C1 bus and address
0x68 are used; no internal STM32 RTC or additional I2C pins are required.
Keep I2C pull-ups at 3.3 V. Set `APP_CONFIG_ENABLE_DS3231` to `0U` to disable
RTC sampling and its command interface.

Use a **3.3 V logic USB-to-UART adapter**: adapter TX to PA10, adapter RX to
PA9, and common GND. Open a terminal at **115200 baud, 8 data bits, no parity,
1 stop bit, no flow control**. Enable local echo if desired. End each command
with Enter (CR, LF, or CRLF). Wait for a response before sending another command.

```text
TIME SET 2026-10-24 14:32:05
TIME?
HELP
```

Replace the example with the actual current date/time. Commands are case-sensitive.
Dates are validated, including leap days, in the range 2000-2099; hours use
24-hour format. Weekday is calculated automatically. The stored time has no
time-zone metadata: use local time consistently (e.g. IST) or UTC consistently.
No build-time date is written automatically, and no internet synchronization is
performed. The driver reads both 12-hour and 24-hour clocks, and writes 24-hour mode.

Without a battery, the clock must be set again after RTC power is removed.
Resetting just the STM32 preserves a still-powered RTC. Startup preserves the
oscillator-stop flag; unset time is reported as `RTC TIME NOT SET` on the OLED
and `timestamp INVALID` in serial samples. I2C faults display `RTC ERROR`.
Sensor sampling continues in both cases. Invalid timestamps are zeroed and their
validity flag is cleared, so consumers must check `WEATHER_DATA_VALID_TIMESTAMP`.
The date occupies OLED row 0 and time row 5; existing sensor rows are retained.
Each serial `Sample:` timestamp applies to the following sensor lines and marks
the start of that sequential sampling cycle, not simultaneous acquisition.

USART1 reception is interrupt-driven and buffered while sensor reads block.
The application enables its IRQ; the handler is in a preserved USER CODE section.
Leave CubeMX USART1 NVIC generation disabled to avoid a duplicate handler.
Overflow or UART errors discard the command: press Enter and resend when prompted.
Neither I2C operations nor command execution runs in the UART interrupt.

Driver reference: [Analog Devices DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf).

### RTC verification

Run host tests with `sh tests/run_rtc_tests.sh`. They use a fake HAL/register map
and do not require the board. Build firmware with the usual CMake presets.
On hardware: confirm address 0x68 with the optional scanner, set time, check that
seconds advance, reset the STM32 while keeping the RTC powered, then remove all
power and confirm unset-time reporting. Also unplug the RTC and confirm sensors
continue with invalid timestamps. These hardware checks require the physical board.
