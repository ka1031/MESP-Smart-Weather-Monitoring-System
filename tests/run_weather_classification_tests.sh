#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Itests/fakes -IApplication/Inc \
  tests/test_weather_classification.c Application/Src/weather_classification.c -o "$test_dir/classifier"
"$test_dir/classifier"
for source in weather_classification esp32_link weather_csv; do
  cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Itests/fakes -IApplication/Inc \
    -c "Application/Src/$source.c" -o "$test_dir/$source.o"
done
c++ -std=c++11 -Wall -Wextra -Wpedantic -Werror -Itests/fakes -IApplication/Inc \
  tests/test_weather_end_to_end.cpp "$test_dir/"*.o -o "$test_dir/end_to_end"
"$test_dir/end_to_end"
