#!/bin/sh
set -eu
cd "$(dirname "$0")"
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer test_receiver.cpp -o "$test_dir/test_receiver"
ASAN_OPTIONS=detect_leaks=0 "$test_dir/test_receiver"
