# MESP Smart Weather Monitoring System

An embedded smart weather monitoring system based on the STM32F103C8T6.

## Features

- Temperature, humidity and pressure monitoring
- Ambient light measurement
- Rain detection
- Wind speed measurement
- Real-time clock and timestamping
- 20×4 LCD display
- Weather status classification
- LED and buzzer alerts
- Local microSD data logging
- ESP32 Wi-Fi communication
- Cloud-based remote monitoring

## Hardware

- STM32F103C8T6 Blue Pill
- BME280
- BH1750
- Analog Rain Sensor
- Hall-effect Anemometer
- DS3231 RTC
- 20×4 I2C LCD
- ESP32
- Green LED
- Red LED
- Buzzer
- MicroSD Card Module

## Project Structure

```text
Core/       STM32 application code
Drivers/    STM32 HAL and peripheral drivers
cmake/      CMake configuration
docs/       Documentation and hardware information
modules/    Individual hardware module development