#include "../MESP_ESP32/weather_receiver.h"
#include <assert.h>
#include <stdio.h>
#include <string>

static const char *golden = "$MESP,1,42,1234,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15*4E4C\r\n";
static bool feed(WeatherReceiver &r, const std::string &s, uint32_t now = 100) {
  bool accepted = false;
  for (char c : s) accepted = r.feed(c, now) || accepted;
  return accepted;
}
static std::string frame(const std::string &body) {
  char end[8]; snprintf(end, sizeof(end), "*%04X\r\n", WeatherReceiver::crc(body.data(), body.size()));
  return "$" + body + end;
}
int main() {
  WeatherReceiver r;
  assert(!r.hasSample && !r.fresh(0));
  assert(WeatherReceiver::crc("123456789", 9) == 0x29b1);
  assert(feed(r, golden));
  assert(r.sample.sequence == 42 && r.sample.values[0] == -0.25);
  assert(r.sample.flags == 15 && r.sample.timeValid);
  assert(!r.sample.classificationAvailable && !strcmp(r.sample.weatherStatus, "UNKNOWN"));
  assert(r.fresh(5100) && !r.fresh(5101));
  std::string corrupt = golden; corrupt[20] ^= 1;
  assert(!feed(r, corrupt)); assert(r.accepted == 1 && r.rejected == 1);
  assert(r.sample.sequence == 42); // A bad packet never replaces good data.
  assert(feed(r, "noise$partial" + std::string(golden)));
  assert(!feed(r, "$" + std::string(400, 'x') + "\r\n"));
  assert(feed(r, golden));
  feed(r, "$MESP", 500); r.expire(751);
  assert(!feed(r, std::string(golden).substr(5), 752));
  assert(feed(r, golden, 800));
  assert(feed(r, frame("MESP,1,4294967295,4294967295,,0,,,,,,,,0")));
  for (bool present : r.sample.present) assert(!present);
  assert(!r.sample.timeValid);
  assert(feed(r, frame("MESP,1,0,0,,0,,,,,,,,7"))); // Valid group, missing value allowed.
  const char *invalid[] = {
    "MESP,2,1,1,,0,,,,,,,,0", "MESP,1,4294967296,1,,0,,,,,,,,0",
    "MESP,1,1,1,,0,,,,,,,,16", "MESP,1,1,1,,0,,,,,,,,0,extra",
    "MESP,1,1,1,,0,,,,,,,0", "MESP,1,1,1,,0,nan,,,,,,,1",
    "MESP,1,1,1,,0,20.00,,,,,,,0", "MESP,1,1,1,,0,20.00,101.00,,,,,,1",
    "MESP,1,1,1,,0,,,,,4096,20,0,4", "MESP,1,1,1,,0,,,,,1,20,2,4",
    "MESP,1,1,1,2026-02-29 00:00:00,1,,,,,,,,8",
    "MESP,1,1,1,2026-10-28 24:00:00,1,,,,,,,,8",
    "MESP,1,1,1,,1,,,,,,,,8", "MESP,1,1,1,,0,,,,,,,,8",
    "MESP,1,1,1,,0,2e1,,,,,,,1"
  };
  for (const char *body : invalid) assert(!feed(r, frame(body)));
  assert(feed(r, frame("MESP,1,1,1,2028-02-29 23:59:59,1,,,,,,,,8")));
  assert(feed(r, golden, UINT32_MAX - 50));
  assert(r.fresh(20));
  feed(r, "$MESP", UINT32_MAX - 50); r.expire(300);
  assert(feed(r, golden, 301));
  const std::string v2base="MESP,2,42,1234,2026-10-28 14:32:05,1,-0.25,62.50,1008.25,123.75,3000,27,0,15,";
  assert(feed(r, frame(v2base + "SEVERE,2")));
  assert(r.sample.classificationAvailable && !strcmp(r.sample.weatherStatus, "SEVERE") && r.sample.weatherReasons == 2);
  assert(feed(r, frame(v2base + "UNFAVORABLE,12")));
  assert(feed(r, frame(v2base + "NORMAL,0")));
  assert(feed(r, frame(v2base + "UNKNOWN,64")));
  for (const char *bad : {"NORMAL,1", "UNKNOWN,1", "UNFAVORABLE,0", "SEVERE,8", "SEVERE,129", "CLEAR,0", "NORMAL"})
    assert(!feed(r, frame(v2base + bad)));
  assert(feed(r, golden));
  assert(!r.sample.classificationAvailable && r.sample.weatherReasons == 0);
  // Exercise arbitrary bytes and frame boundaries under sanitizers.
  uint32_t random = 42;
  for (unsigned i = 0; i < 100000; ++i) {
    random = random * 1664525U + 1013904223U;
    r.feed(char(random >> 24), i);
  }
  assert(feed(r, golden, 100001));
  puts("ESP32 receiver: CRC, fields, ranges, time, overflow, resync, stale data and noise passed");
}
