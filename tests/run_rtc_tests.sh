#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Drivers/Devices/Inc" -I"$repo_dir/Application/Inc" \
  "$repo_dir/tests/test_rtc.c" "$repo_dir/Drivers/Devices/Src/ds3231.c" \
  "$repo_dir/Application/Src/rtc_console.c" "$repo_dir/Application/Src/weather_app.c" -o "$test_dir/test_rtc"
"$test_dir/test_rtc"
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Drivers/Devices/Inc" \
  "$repo_dir/tests/test_rtc_display.c" "$repo_dir/Drivers/Devices/Src/ssd1306.c" \
  -o "$test_dir/test_rtc_display"
"$test_dir/test_rtc_display"
