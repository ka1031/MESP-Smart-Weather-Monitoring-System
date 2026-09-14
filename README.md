# MESP Smart Weather Monitoring System

A modular weather-monitoring prototype based on the STM32F103C8T6 and STM32
HAL. The project intentionally uses a simple polling-based superloop; it does
not currently require an RTOS or an additional application framework.

## Current prototype

- BME280 temperature, humidity, and pressure readings
- BH1750 ambient-light readings
- Rain-module analog averaging and active-low digital wet/dry detection
- SSD1306 128x64 OLED output
- Matching diagnostics through USART1 at 115200 baud
- PC13 heartbeat toggled after every one-second sample

The DS3231, data logging, networking, and cloud features remain future work.

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

Peripheral assignments are controlled by `MESP_WeatherStation.ioc`. External
I2C devices such as the DS3231 do not require the STM32 internal RTC peripheral.

### Temporary I2C scanner

`Application/Src/temporary_i2c_scanner.c` remains available as a temporary
hardware-debug tool. It is disabled by default. Set
`APP_CONFIG_ENABLE_I2C_SCANNER` to `1U` in `Application/Inc/app_config.h` to run
one startup scan and print responding addresses through USART1.

## Prototype wiring

- BME280, BH1750, and SSD1306: SCL to PB8 and SDA to PB9
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

Flashing is intentionally not embedded in the build system yet. Use the
existing ST-Link/OpenOCD or STM32CubeProgrammer workflow for the board.

## Adding a device implementation

Implement and test one driver at a time under `Drivers/Devices`. After a driver
works independently, create its handle in `weather_app.c`, initialize it from
`WeatherApp_Init`, and schedule non-blocking or short polling operations from
`WeatherApp_Run`. Keep device-specific registers and conversion formulas out of
application code.
