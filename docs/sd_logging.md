# SD CSV logging

Logging is enabled by default. On boot, the station initializes the card using
the SPI path validated by the read-only diagnostic, mounts FAT16/FAT32, creates
a new file, and saves the first sensor sample. Further rows are saved every
five seconds. Sensor sampling and OLED updates remain approximately once a
second. SD operations are synchronous, so writes or faults can delay a sample.

## Flash and use

Build both presets with `cmake --build --preset Debug` and
`cmake --build --preset Release`. Each build generates matching `.elf`, `.bin`,
and `.hex` files in its own directory. Flash the ELF using ST-Link or STM32CubeProgrammer. Raw BIN files load at `0x08000000`.

With the card inserted, power-cycle the station. The bottom OLED row alternates
between rain percentage and `SD LOG 1`, `SD LOG 2`, etc. The count advances only
after an entire CSV row has been written and synchronized. It is the number of
saved rows in the current file, not the number of seconds or sensor samples.

The card needs an existing FAT16/FAT32 filesystem. Partitioned cards and FAT
volumes starting at sector zero are supported. exFAT is not supported. An
unrecognized filesystem produces `SD FORMAT F13`; the firmware never formats
the card. Back up existing contents before any user-performed reformatting.

Wiring remains CS=PA4, SCK=PA5, MISO=PA6, MOSI=PA7, with common ground and the
appropriate HW-125 supply. CubeMX owns the pins and `hspi1`. The SPI clock stays
at the tested 281.25 kHz for both initialization and transfers. Reads verify
CRC16; writes send CRC16, check the card's acceptance, wait for programming to
finish, and check CMD13 status.

## Files and contents

Files are named `LOG00001.CSV`, `LOG00002.CSV`, and so on. Each boot or restart
creates a file above the highest existing log number. Creation uses
`FA_CREATE_NEW`, so an existing file is never truncated. At LOG99999, logging
stops rather than reusing a name. A repeated START while already logging keeps
the current file.

Columns are `uptime_ms`, `timestamp`, `time_valid`, `temperature_c`,
`humidity_pct`, `pressure_hpa`, `light_lux`, `rain_raw`, `rain_pct`, `rain_wet`,
and `valid_flags`.

The timestamp comes from the DS3231 in the same sensor sample. If its time is
invalid, the timestamp is blank and `time_valid` is zero; sensor logging still
works. Unavailable sensor readings are blank. The validity bits are BME280=1,
BH1750=2, rain=4, RTC=8. Numeric readings use two decimal places. Uptime is a
32-bit millisecond counter and wraps after approximately 49.7 days.

## Serial commands and removal

Use the existing USART1 console at 115200 baud, 8N1:

- `SD?`: report state, filename and saved rows, or the error and transport details.
- `SD STOP`: synchronize and close the file. Wait for `STOPPED; safe to remove card`
  before removing the card or powering down.
- `SD START`: initialize/mount again and create a new file on the next sample.
- `HELP`: list these commands and the existing RTC commands.

Every row is synchronized, reducing exposure to abrupt power loss, but this
does not make removal during an active write safe. After an I/O error the
firmware stops writing, unmounts, and retains the error. It does not claim a
successful close. Fix the fault, then use START or reboot. A full card can leave
a partial final CSV row; that row is not counted as saved.

OLED errors include `SD FULL`, `SD FORMAT F13`, `SD FILE F<number>`, and transport
messages such as `SD NO R1 C0`, `SD DATA CRC C17`, or `SD WRITE REJECT C24`.
`SD?` includes the command response, data token, and HAL error details.

## Configuration and validation

`Application/Inc/app_config.h` holds `APP_CONFIG_SD_LOG_PERIOD_MS` (5000).
To return to the standalone read-only diagnostic, set
`APP_CONFIG_ENABLE_SD_LOGGING=0` and `APP_CONFIG_ENABLE_SD_DIAGNOSTIC=1`, then
rebuild and flash. The two modes are mutually exclusive. Normal logging mode
uses START/STOP; it does not reset an actively mounted card with `SD TEST`.

Run `sh tests/run_sd_tests.sh`, `sh tests/run_sd_logging_tests.sh`, and
`sh tests/run_rtc_tests.sh`. Tests use temporary disk-image files, never a
physical card. They cover SDHC/SDSC protocol, CRCs, errors, FAT16/FAT32 and MBR
partition layouts, CSV contents, cadence and timer rollover, restart/reboot,
existing-log preservation, rejected writes, full cards and blank media.
`fsck.fat -n` checks the resulting filesystems. SD initialization and CSV logging have been confirmed working on the board.
