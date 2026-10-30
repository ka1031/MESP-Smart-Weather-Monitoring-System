# ESP32 setup

Firmware for the 30-pin ESP32 DevKit V1 (ESP-WROOM-32). It receives readings
from the STM32 over UART and serves a local Wi-Fi dashboard.

## PlatformIO

1. Open the `ESP32` folder in VS Code. It contains `platformio.ini`.
2. Connect the board with a USB data cable.
3. Select the ESP32 serial port under PlatformIO Devices.
4. Build and upload using the `esp32dev` environment.
5. Open Serial Monitor at 115200 baud. Press EN to view startup messages.

The project pins Espressif32 platform 6.12.0 and Arduino-ESP32 2.0.17.
WiFi and WebServer are included with the framework; no extra libraries are needed.

If no USB serial port appears, check the cable and USB connection. Do not use
`/dev/ttyS0` as a substitute. If upload stops at `Connecting...`, hold BOOT
until writing starts, then release it.

## Arduino IDE

Keep `MESP_ESP32.ino` and `weather_receiver.h` together in `MESP_ESP32/`.
Install **esp32 by Espressif Systems**, version **2.0.17**, through Boards Manager.
Open the sketch and select **ESP32 Dev Module**, upload speed **115200**,
**4 MB flash** for the standard WROOM module, default partitions and PSRAM disabled.
Select the board's USB port and upload.

[Arduino-ESP32 installation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

## Connections

Connect with both boards powered off. Use the printed GPIO labels.

- STM32 PA2 (USART2 TX) → ESP32 GPIO16 (RX2).
- STM32 GND → ESP32 GND.
- ESP32 GPIO17 (TX2) → STM32 PA3 (USART2 RX) is reserved for future commands
  and is optional for the current receiver.

UART uses 3.3 V logic, 115200 baud, 8N1. Power the ESP32 through USB and the
STM32 through its normal supply. Share ground; do not join separately powered
supply rails. Leave ESP32 GPIO1/GPIO3 available for USB programming.

The STM32 must run the UART-sender firmware. Its SD and sensor wiring is unchanged.

## Local dashboard

- Wi-Fi network: **MESP-Weather**
- Password: **MespWeather32**
- Address: **http://192.168.4.1/** (also printed at startup)

Stay connected if the phone reports no internet access. The ESP32 creates a
local hotspot; it does not connect to a router or publish readings to the cloud.
The hotspot name and password can be changed in the sketch before uploading.

Serial Monitor prints `RX OK` for each accepted packet, normally once per second.
`NA` means a reading is unavailable. The dashboard reports `Live` while valid
packets arrive and `STALE` after five seconds without one. Old readings remain
visible with the stale warning. `WAITING` means no valid packet has arrived yet.

Check that displayed values match the STM32 OLED. Disconnecting and reconnecting
the UART signal should produce a stale warning followed by automatic recovery.
Increasing rejected-packet counts can indicate damaged data or a protocol mismatch.

## Data interface

`/api/readings` returns JSON. The `values` array contains temperature (°C),
humidity (%), pressure (hPa), light (lux), rain ADC, plate wetness (%), and
wet/dry state (1/0), in that order. Missing values are `null`. Check `has_sample`
and `fresh` before using readings. Timestamps use the STM32 RTC's local time.

Rain percentage is plate wetness, not rainfall depth. Wind sensing is excluded
from the project. Classification is calculated by the STM32 and shown on this page. Cloud publishing
is pending.

## Classification update

Upload this ESP32 version before flashing the STM32 classification firmware.
The receiver accepts v1 packets without classification and v2 packets with status
and reason fields. Both boards need updated firmware to display classification.
No wiring changes are required. The JSON adds `classification_available`,
`weather_status` and `weather_reasons`. UNKNOWN indicates startup or a required
sensor fault; it must not be treated as NORMAL.

Reason bits are 1=high temperature, 2=low temperature, 4=high humidity,
8=rain, 16=missing sensor, 32=invalid data, and 64=startup/checking.
The dashboard displays the STM32 result; it does not independently classify.

## Tests

Run `sh tests/run_tests.sh` for the receiver parser tests. Build with `pio run`.
Build results and hardware-check status are recorded in `VERIFICATION.txt`.
