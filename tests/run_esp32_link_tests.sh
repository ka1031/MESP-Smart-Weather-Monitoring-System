#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$repo_dir/tests/fakes" -I"$repo_dir/Application/Inc" \
  "$repo_dir/tests/test_esp32_link.c" "$repo_dir/Application/Src/esp32_link.c" \
  "$repo_dir/Application/Src/weather_csv.c" -o "$test_dir/test_esp32_link"
"$test_dir/test_esp32_link" > "$test_dir/packets.txt"
python3 - "$test_dir/packets.txt" <<'PY'
import binascii, csv, pathlib, sys
packets = pathlib.Path(sys.argv[1]).read_bytes().splitlines(keepends=True)
assert len(packets) == 4
for packet in packets:
    assert packet.startswith(b'$') and packet.endswith(b'\r\n') and len(packet) < 320
    payload, checksum = packet[1:-2].rsplit(b'*', 1)
    assert len(checksum) == 4 and int(checksum, 16) == binascii.crc_hqx(payload, 0xffff)
    fields = next(csv.reader([payload.decode('ascii')]))
    assert len(fields) == 14 and fields[:2] == ['MESP', '1']
    assert 0 <= int(fields[2]) <= 0xffffffff and 0 <= int(fields[3]) <= 0xffffffff
print('ESP32 UART: packets, independent CRC/CSV validation, bounds, missing data, failures and rollover passed')
PY
