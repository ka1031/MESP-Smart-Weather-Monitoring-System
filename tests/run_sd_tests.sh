#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Drivers/Devices/Inc" -I"$repo_dir/Application/Inc" \
  "$repo_dir/tests/test_sd_probe.c" "$repo_dir/Drivers/Devices/Src/sd_probe.c" \
  "$repo_dir/Application/Src/sd_diagnostic.c" -o "$test_dir/test_sd"
"$test_dir/test_sd"
