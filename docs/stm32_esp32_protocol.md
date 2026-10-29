# STM32 to ESP32 weather link, version 1

The STM32 sender and the separate `ESP32/` receiver are implemented. The receiver
provides a local Wi-Fi page; cloud publishing remains future work. The sensors, RTC console, OLED and
SD logger continue to operate independently of an ESP32 being connected.

## Wiring and serial settings

- STM32 PA2 (USART2 TX) to ESP32-WROOM GPIO16 (RX2).
- STM32 PA3 (USART2 RX) from ESP32-WROOM GPIO17 (TX2).
- Common ground. Signals are 3.3 V logic.
- 115200 baud, 8 data bits, no parity, one stop bit, no flow control.

The current protocol sends in the STM32-to-ESP32 direction only; PA3 is reserved
for future return traffic. There are no commands or acknowledgements on USART2.
USART1 on PA9/PA10 remains the human-readable debug and RTC/SD command console.
CubeMX already configures USART2; the sender uses its generated `huart2` handle
without changing GPIO, clocks, DMA or interrupts.

## Frame format

Each ASCII frame starts with `$`, ends with CRLF, and has a four-digit uppercase
hexadecimal checksum after `*`. The terminating C-string NUL is not transmitted.

```text
$MESP,1,sequence,uptime_ms,timestamp,time_valid,temperature_c,humidity_pct,pressure_hpa,light_lux,rain_raw,rain_pct,rain_wet,valid_flags*CRC16\r\n
```

The `\r\n` above represents actual carriage-return and line-feed bytes.
The fields between `$` and `*`, in order, are:

1. `MESP`: fixed message identifier.
2. `1`: protocol version.
3. `sequence`: unsigned 32-bit transmission-attempt number, starting at zero.
4. `uptime_ms`: unsigned 32-bit tick captured at the beginning of the sample cycle.
5. `timestamp`: `YYYY-MM-DD HH:MM:SS` from the DS3231, or an empty field.
6. `time_valid`: 1 when the RTC timestamp is valid; otherwise 0.
7. `temperature_c`: degrees Celsius, two decimal places, or empty.
8. `humidity_pct`: relative humidity percent, two decimal places, or empty.
9. `pressure_hpa`: hPa, two decimal places, or empty.
10. `light_lux`: lux, two decimal places, or empty.
11. `rain_raw`: ADC reading, normally 0–4095, or empty.
12. `rain_pct`: estimated plate wetness, normally 0–100, or empty. Not rainfall in mm.
13. `rain_wet`: 0=dry, 1=wet, or empty.
14. `valid_flags`: decimal bit mask: BME280=1, BH1750=2, rain=4, RTC=8.

Do not discard empty fields when splitting the CSV. A missing/invalid sensor
produces empty values rather than stale readings or fabricated zeros. Non-finite
and out-of-format-range float values are also blanked; the validity flags retain
the original sensor-group validity, so receivers must check field presence too.
There is no wind field or weather-classification field. No `NORMAL` status is
invented while classification is unimplemented.

The timestamp has the RTC's configured local time and no timezone suffix. All
readings belong to one sequential sampling cycle, not simultaneous acquisitions.
Sequence and uptime wrap modulo 2^32 and restart on MCU/app initialization. A
sequence gap may indicate a dropped transmission; a reset also resets sequence.

## Checksum and receiver requirements

CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`, no reflection,
and no final XOR. Calculate over the exact ASCII bytes after `$` and before `*`;
exclude both delimiters, the checksum text, and CRLF.

This complete example has checksum `4E4C` (append actual CRLF when sending):

```text
$MESP,1,42,1234,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15*4E4C
```

The receiver should accumulate up to 319 bytes including CRLF, plus one byte
for a local NUL terminator if needed. A fresh `$` restarts frame collection so
a reset or partial transmission cannot permanently desynchronize the stream.
Discard oversized frames, invalid checksums, unsupported versions, and records
with a field count other than 14. Validate numeric ranges before using values.
On an incomplete frame, wait for the next `$` or discard after a finite timeout.
CRC detects corruption, not sender identity or guaranteed delivery.

## Timing and error behavior

One packet is attempted after each completed sensor sample, normally once per
second, before SD logging. There is no backlog, acknowledgement wait, or retry.
The sequence advances for every attempt, including failures. The sender uses a
bounded polling transmit with a 50 ms timeout. A maximum-length frame takes
under 28 ms at 115200 baud; typical frames are shorter. SD and sensor operations
can still delay future sample cycles, so this is not a hard real-time stream.

UART transmit failures are reported on USART1 when the link changes from
successful transmission to failure, and once when transmission recovers.
Sensor sampling, OLED updates and SD logging continue after a transmit error.
`HAL_OK` only confirms bytes left the STM32 UART: an unplugged/unprogrammed ESP32
does not necessarily cause an error. Actual reception must be tested separately.

## Build and verification

`APP_CONFIG_ENABLE_ESP32_LINK` in `Application/Inc/app_config.h` defaults to 1.
Set it to 0 to disable forwarding. `APP_CONFIG_ESP32_UART_TIMEOUT_MS` is 50.
The packet rate follows `APP_CONFIG_SAMPLE_PERIOD_MS`; the SD log period is
independent. Framing reuses the existing weather CSV formatter to conserve flash.

Run `sh tests/run_esp32_link_tests.sh` and `sh tests/run_rtc_tests.sh`.
Tests cover a golden packet, independent Python CRC/CSV decoding, exact-size
and undersized buffers, absent/non-finite sensor data, maximum-size fields,
sequence rollover, incorrect UART binding, HAL errors, and application sampling
and SD calls during UART failure. SD regressions are in `run_sd_tests.sh` and
`run_sd_logging_tests.sh`.

Build Debug and Release with their existing CMake presets. Both generate
ELF/BIN/HEX files. Flash the STM32 with the updated ELF using the existing
workflow. Flash the separate ESP32 sketch following
[ESP32 upload instructions](../ESP32/UPLOAD_INSTRUCTIONS.md), then verify packets
in its USB serial monitor and compare values with the OLED and SD log.
