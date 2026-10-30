#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for sd_mode in logging diagnostic disabled esp_disabled auto_set; do
logging=0
diagnostic=0
esp=1
auto_set=0
case "$sd_mode" in auto_set) logging=1; auto_set=1;; logging) logging=1;; diagnostic) diagnostic=1;; esp_disabled) logging=1; esp=0;; esac
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -DAPP_CONFIG_RTC_AUTO_SET=$auto_set -DAPP_CONFIG_ENABLE_SD_DIAGNOSTIC=$diagnostic -DAPP_CONFIG_ENABLE_SD_LOGGING=$logging -DAPP_CONFIG_ENABLE_ESP32_LINK=$esp \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Drivers/Devices/Inc" -I"$repo_dir/Application/Inc" \
  "$repo_dir/tests/test_rtc.c" "$repo_dir/Drivers/Devices/Src/ds3231.c" \
  "$repo_dir/Application/Src/weather_classification.c" "$repo_dir/Application/Src/rtc_console.c" "$repo_dir/Application/Src/weather_app.c" \
  "$repo_dir/Application/Src/esp32_link.c" "$repo_dir/Application/Src/weather_csv.c" -o "$test_dir/test_rtc"
"$test_dir/test_rtc"
done
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Drivers/Devices/Inc" \
  "$repo_dir/tests/test_rtc_display.c" "$repo_dir/Drivers/Devices/Src/ssd1306.c" \
  -o "$test_dir/test_rtc_display"
"$test_dir/test_rtc_display"
