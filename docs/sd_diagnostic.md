# Read-only SD hardware diagnostic

Normal firmware now enables [CSV logging](sd_logging.md). To build the standalone
diagnostic described below, set `APP_CONFIG_ENABLE_SD_LOGGING` to `0U` and
`APP_CONFIG_ENABLE_SD_DIAGNOSTIC` to `1U` in `Application/Inc/app_config.h`.

This stage initializes the card and reads sector zero, checking its CRC16.
It does not mount a filesystem, create files, write sectors, erase, or format.
An `SD READ OK` result verifies initialization and one sector read at the slow
startup clock; it does not yet verify filesystem compatibility, writes, or a
faster SPI clock.

## Hardware and CubeMX setup

- HW-125 CS to PA4 (`SD_CS`), SCK to PA5, MISO to PA6, MOSI to PA7.
- Common ground. For the HW-125 version with an AMS1117-3.3 regulator, supply
  its VCC from the specified 5 V input; SPI signals remain at 3.3 V.
- SPI1 full-duplex master, mode 0, 8-bit, MSB first, software NSS.
- Prescaler 256 throughout this diagnostic: 281.25 kHz at the current 72 MHz PCLK2.
- PA4 push-pull output initially high; PA5/PA7 alternate-function push-pull;
  PA6 input with pull-up.

CubeMX owns `hspi1`, GPIO, clocks, and `MX_SPI1_Init`. The application binds the
generated handle and CS definitions in `main.c` USER CODE. The driver adjusts
only the existing handle's baud prescaler and reinitializes it through HAL;
there is no second SPI handle or project-owned GPIO initializer.

## Build and test on the board

1. Build with `cmake --build --preset Debug`.
2. Flash `build/Debug/MESP_WeatherStation.elf` with your normal programmer.
   If using the raw `.bin` artifact, the application starts at `0x08000000`.
3. Power-cycle with the card inserted. The test runs once at boot.
4. Watch the bottom OLED row: it alternates every four seconds between rain
   percentage and the SD result. Existing measurements continue afterward.
5. Report the exact SD message. For more detail, use USART1 at 115200 baud,
   8 data bits, no parity, one stop bit, and send `SD?` followed by Enter.
   Send `SD TEST` followed by Enter to retry without rebooting.

The test is synchronous and can briefly delay sampling. The total transport
deadline is three seconds, with up to one short in-flight HAL transfer beyond
that limit. Failed attempts stay latched; they do not repeatedly block the
weather station. Reset attempts are bounded, and only `SD TEST` or a reboot
starts another full test.

## OLED results

- `SD READ OK`: card initialized and sector zero passed its CRC16 check.
- `SD NO R1 C0`: no valid CMD0 response after bounded retries. SPI transfers
  completed, but this alone cannot identify a wiring, power, module or card fault.
- `SD SPI TIME C0`: HAL SPI transfer timed out during the CMD0 stage.
- `SD SPI FAIL C...`: HAL SPI returned another transfer error.
- `SD BAD R1 C0`: CMD0 returned an unexpected response; the serial report
  includes the raw response. An always-low MISO can produce this result.
- `SD BUSY C...`: the selected card never released MISO before a command.
- `SD BAD ECHO C8`: CMD8's voltage/check-pattern reply was invalid.
- `SD INIT TIME C41`: the card did not finish initialization in time.
- `SD BAD OCR C58`: the reported power-up/voltage information was invalid.
- `SD NO TOKEN C17`: no sector-start token arrived within the read timeout.
- `SD BAD TOKEN C17`: a sector error/unexpected token was received.
- `SD DATA CRC C17`: sector data did not match its transmitted checksum.
- `SD DEADLINE C...`: the overall operation reached its time limit.
- `SD SPI INIT C255` / `SD BAD HANDLE C255`: diagnostic setup failed before
  a card command. `C255` means no command has been attempted; it is not CMD255.

Serial output includes command, R1 response, token, last received byte, HAL
status, HAL error flags, and elapsed time. HAL status values are 0=OK, 1=ERROR,
2=BUSY, 3=TIMEOUT. An `FF` response/token can mean it was never obtained.
After success, the last two sector bytes are reported as information only;
the diagnostic does not infer FAT compatibility from a `55 AA` signature.

Set `APP_CONFIG_ENABLE_SD_DIAGNOSTIC` to `0U` to disable the boot test, display
row and SD console commands. RTC and weather functionality remain available.

## Automated checks

Run `sh tests/run_sd_tests.sh` and `sh tests/run_rtc_tests.sh`.
The host emulator runs the real read-only driver and diagnostic wrapper;
unexpected commands, including write/erase commands, fail the test. Cases cover
SDHC and SDSC v1/v2, missing/stuck/busy cards, reset retries, SPI errors, response
and token errors, data CRC, initialization/overall deadlines, timer rollover,
latched results and manual retry. RTC/application tests run with the diagnostic
both enabled and disabled and cover command dispatch and OLED alternation.

These checks cannot measure the physical board's supply or SPI waveforms.
The board has passed this read-only test. Filesystem mounting and CSV logging
are implemented in the normal logging mode and require a separate write test.
