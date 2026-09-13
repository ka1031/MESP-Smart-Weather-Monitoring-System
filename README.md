# MESP Smart Weather Monitoring System

A modular weather-monitoring prototype based on the STM32F103C8T6 and STM32
HAL. The project intentionally uses a simple polling-based superloop; it does
not currently require an RTOS or an additional application framework.

## Planned capabilities

- Temperature, humidity, and pressure measurement with BME280
- Ambient-light measurement with BH1750
- Analog rain detection
- Date and time from DS3231
- UART diagnostics
- Local status output, followed later by LCD support
- Future wind sensing, microSD logging, ESP32 communication, and cloud upload

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

The current device source files are deliberate scaffolds. They validate basic
arguments and report `DEVICE_STATUS_NOT_IMPLEMENTED` until each module is
implemented and tested. They do not return simulated sensor readings.

## Current CubeMX configuration

- STM32F103C8T6 at 72 MHz from HSE and PLL
- I2C1 at 100 kHz on PB6/PB7
- USART1 at 115200 baud on PA9/PA10
- USART2 at 115200 baud on PA2/PA3
- SWD on PA13/PA14

Peripheral assignments are controlled by `MESP_WeatherStation.ioc`. External
I2C devices such as the DS3231 do not require the STM32 internal RTC peripheral.

## Building

Install the Arm GNU embedded toolchain, CMake 3.22 or newer, and Ninja. Ensure
`arm-none-eabi-gcc`, `cmake`, and `ninja` are available on the command line.

Configure and build the Debug version from the repository root:

```sh
cmake --preset Debug
cmake --build --preset Debug
```

The resulting firmware image is generated under `build/Debug`. A release build
can be produced with:

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
