#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for tool in cc mkfs.fat fsck.fat truncate python3; do
  command -v "$tool" >/dev/null || { echo "Missing test dependency: $tool"; exit 1; }
done
cc -funsigned-char -std=c11 -Wall -Wextra -Wpedantic -Werror -g \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/tests" \
  -I"$repo_dir/Drivers/Devices/Inc" -I"$repo_dir/Application/Inc" \
  -I"$repo_dir/Middlewares/Third_Party/FatFs" \
  "$repo_dir/tests/test_sd_logging.c" "$repo_dir/tests/sd_card_emulator.c" \
  "$repo_dir/Drivers/Devices/Src/sd_probe.c" "$repo_dir/Application/Src/sd_diskio.c" \
  "$repo_dir/Application/Src/sd_logger.c" "$repo_dir/Application/Src/weather_csv.c" \
  "$repo_dir/Middlewares/Third_Party/FatFs/ff.c" -o "$test_dir/test_sd"
# Only temporary regular files: these commands never touch a physical disk.
truncate -s 16M "$test_dir/fat16.img"
mkfs.fat -F 16 "$test_dir/fat16.img" >/dev/null
"$test_dir/test_sd" "$test_dir/fat16.img" fat16
fsck.fat -n "$test_dir/fat16.img"
truncate -s 64M "$test_dir/fat32.img"
mkfs.fat -F 32 "$test_dir/fat32.img" >/dev/null
"$test_dir/test_sd" "$test_dir/fat32.img" fat32
fsck.fat -n "$test_dir/fat32.img"
# Typical card layout: an MBR with a FAT32 partition starting at sector 2048.
truncate -s 64M "$test_dir/volume.img"
mkfs.fat -F 32 "$test_dir/volume.img" >/dev/null
python3 - "$test_dir/volume.img" "$test_dir/partitioned.img" <<'PY'
import pathlib, shutil, struct, sys
volume, image = map(pathlib.Path, sys.argv[1:])
sectors = volume.stat().st_size // 512
mbr = bytearray(512)
mbr[450] = 0x0C
struct.pack_into('<II', mbr, 454, 2048, sectors)
mbr[510:512] = b'\x55\xaa'
with image.open('wb') as dest, volume.open('rb') as src:
    dest.write(mbr)
    dest.seek(2048 * 512)
    shutil.copyfileobj(src, dest)
PY
"$test_dir/test_sd" "$test_dir/partitioned.img" fat32
python3 - "$test_dir/partitioned.img" "$test_dir/volume.img" <<'PY'
import shutil, sys
with open(sys.argv[1], 'rb') as src, open(sys.argv[2], 'wb') as dest:
    src.seek(2048 * 512)
    shutil.copyfileobj(src, dest)
PY
fsck.fat -n "$test_dir/volume.img"
truncate -s 16M "$test_dir/blank.img"
"$test_dir/test_sd" "$test_dir/blank.img" blank
