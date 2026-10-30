# Weather classification

The STM32 assigns a condition status after each sensor sample. These are
configurable prototype thresholds, not a forecast or official warning system.
Wind sensing is excluded.

## Default rules

- `UNFAVORABLE`: temperature at or above 35 °C or at or below 10 °C;
  humidity at or above 85%; or the rain module's digital input reports wet.
- `SEVERE`: temperature at or above 40 °C or at or below 5 °C.
- `NORMAL`: none of the configured conditions are active.
- `UNKNOWN`: startup, missing required readings, or invalid required readings.

The highest active level wins. All active reasons are recorded, so high
humidity and detected rain can be reported together. Rain alone does not imply
severe conditions: the plate measures wetness, not rainfall depth or intensity.
Pressure and light remain available as measurements but do not drive status.
Absolute station pressure depends on altitude; darkness can simply mean night.

Thresholds are in `Application/Inc/app_config.h`, under `APP_CONFIG_WEATHER_*`.
Temperature warning/severe limits must remain ordered, with positive hysteresis
smaller than their separation. Confirmation counts must be between 1 and 255.
Rain detection uses the module's digital comparator; adjust its potentiometer
and verify dry/wet behavior on the actual plate.

## Stable transitions

Startup and upward transitions require three consecutive samples of the same
candidate status. Downward transitions require ten. At the usual sample rate,
these correspond to roughly three and ten seconds; peripheral delays can extend
that time. Any return to the current status cancels a pending transition.

Temperature thresholds have 1 °C hysteresis. For example, high-temperature
warning activates at 35 °C and clears its threshold latch at or below 34 °C;
severe activates at 40 °C and clears at or below 39 °C. Low-temperature warning
clears at or above 11 °C; low-temperature severe clears at or above 6 °C.
Humidity warning clears at or below 80%. Recovery confirmation still applies.

While a transition is pending, the published status and reasons retain the
previous confirmed result. Reasons update immediately when the candidate status
matches the confirmed status. Startup remains `UNKNOWN` until confirmed.

BME280 and rain validity flags are required. Missing readings or non-finite /
out-of-range temperature, humidity or rain readings immediately produce
`UNKNOWN` and reset the classifier. Recovery requires three valid samples.
RTC and light failures do not invalidate classification. A sampling gap over
five seconds restarts confirmation; unsigned tick arithmetic handles rollover.

## Outputs

- OLED: date/time alternate every two seconds with status and the primary
  reason. Sensor rows and the SD/rain row remain visible. Multiple reasons are
  available through the reason mask in logs and on the ESP32 page.
- USART1: status, primary reason and reason mask on each sample.
- SD CSV: `weather_status` and `weather_reasons` follow `valid_flags`.
- USART2: protocol v2 adds the same two fields.
- ESP32: local page, USB serial output and JSON include classification.
  The page marks old classifications stale when packets stop arriving.

Reason bits: 1=high temperature, 2=low temperature, 4=high humidity,
8=rain detected, 16=required sensor missing, 32=invalid required reading,
64=startup/checking. Zero means no active reason. Status codes in STM32 memory
are UNKNOWN=0, NORMAL=1, UNFAVORABLE=2 and SEVERE=3; CSV/UART use their names.

LED and buzzer output are not implemented. PC13 remains the heartbeat LED.

## Firmware update

Upload the updated ESP32 firmware first, then flash the STM32. The new ESP32
receiver accepts v1 measurements with classification marked unavailable, and
v2 with classification. An old ESP32 receiver rejects v2 packets, so both
boards need the update. No CubeMX or wiring change is required.

The SD logger creates a new file after reboot. Existing files retain their
original headers; software reading logs should identify columns by header.

## Verification

- `sh tests/run_weather_classification_tests.sh`: threshold boundaries,
  hysteresis, transition delays, invalid/missing data, gaps, tick rollover,
  and actual STM32 formatter-to-ESP32 parser compatibility.
- `sh tests/run_esp32_link_tests.sh`: frame CRC and buffer boundaries.
- `sh tests/run_rtc_tests.sh`: application, display, RTC and UART integration.
- `sh tests/run_sd_logging_tests.sh`: CSV schema and filesystem regressions.
- `sh ESP32/tests/run_tests.sh`: v1/v2 decoding and malformed records.

On hardware, check startup UNKNOWN followed by a confirmed condition. Wet the
rain plate: after three samples expect UNFAVORABLE / RAIN DETECTED if no stronger
condition is active. Dry it: recovery requires ten consecutive lower-status
samples. Compare the OLED, ESP32 page and the next saved SD row. Hardware
classification checks are pending until the updated firmware is flashed.
