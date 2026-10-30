#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Independent of Arduino so the actual wire parser can also be tested on a PC.
struct WeatherSample {
  uint32_t sequence, uptime;
  char timestamp[20];
  uint8_t flags;
  char weatherStatus[13];
  uint8_t weatherReasons;
  bool classificationAvailable;
  bool timeValid;
  double values[7]; // temperature, humidity, pressure, light, rain raw, %, wet
  bool present[7];
};

class WeatherReceiver {
 public:
  WeatherSample sample{};
  uint32_t accepted = 0, rejected = 0, lastReceived = 0;
  bool hasSample = false;

  void expire(uint32_t now) {
    if (collecting && uint32_t(now - started) > 250) {
      collecting = false; used = 0; ++rejected;
    }
  }
  bool feed(char c, uint32_t now) {
    expire(now);
    if (c == '$') {
      collecting = true; used = 0; started = now;
      buffer[used++] = c; return false;
    }
    if (!collecting) return false;
    if (used >= sizeof(buffer) - 1 || c == '\0') {
      collecting = false; ++rejected; return false;
    }
    buffer[used++] = c;
    if (c != '\n') return false;
    collecting = false; buffer[used] = 0;
    WeatherSample next{};
    if (!decode(next)) { ++rejected; return false; }
    sample = next; hasSample = true; lastReceived = now; ++accepted;
    return true;
  }
  bool fresh(uint32_t now) const {
    return hasSample && uint32_t(now - lastReceived) <= 5000;
  }
  static uint16_t crc(const char *s, size_t n) {
    uint16_t v = 0xffff;
    while (n--) {
      v ^= uint16_t(uint8_t(*s++)) << 8;
      for (unsigned i = 0; i < 8; ++i)
        v = v & 0x8000 ? uint16_t((v << 1) ^ 0x1021) : uint16_t(v << 1);
    }
    return v;
  }
 private:
  char buffer[320]{};
  size_t used = 0;
  uint32_t started = 0;
  bool collecting = false;
  static bool integer(const char *s, uint32_t max, uint32_t &v) {
    if (!*s) return false;
    v = 0;
    for (; *s; ++s) {
      if (*s < '0' || *s > '9') return false;
      uint32_t digit = uint32_t(*s - '0');
      if (digit > max || v > (max - digit) / 10) return false;
      v = v * 10 + digit;
    }
    return true;
  }
  static bool decimal(const char *s, double &v) {
    const char *p = s;
    if (*p == '-') ++p;
    if (*p < '0' || *p > '9') return false;
    while (*p >= '0' && *p <= '9') ++p;
    if (*p++ != '.') return false;
    for (int i = 0; i < 2; ++i) {
      if (*p < '0' || *p > '9') return false;
      ++p;
    }
    if (*p) return false;
    v = strtod(s, nullptr);
    return isfinite(v);
  }
  static bool timestamp(const char *s) {
    if (strlen(s) != 19) return false;
    for (int i = 0; i < 19; ++i) {
      char expected = i == 4 || i == 7 ? '-' : i == 10 ? ' ' : i == 13 || i == 16 ? ':' : 0;
      if (expected ? s[i] != expected : s[i] < '0' || s[i] > '9') return false;
    }
    int year = atoi(s), month = atoi(s + 5), day = atoi(s + 8);
    if (year < 2000 || year > 2099 || month < 1 || month > 12) return false;
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int limit = days[month - 1] + (month == 2 && year % 4 == 0);
    return day >= 1 && day <= limit && atoi(s + 11) < 24 &&
      atoi(s + 14) < 60 && atoi(s + 17) < 60;
  }
  bool decode(WeatherSample &out) {
    if (used < 10 || buffer[used - 2] != '\r') return false;
    char *star = strchr(buffer, '*');
    if (!star || size_t(star - buffer) + 7 != used) return false;
    uint16_t expected = 0;
    for (int i = 1; i <= 4; ++i) {
      char c = star[i];
      if (!(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'F')) return false;
      expected = uint16_t((expected << 4) | (c <= '9' ? c - '0' : c - 'A' + 10));
    }
    if (crc(buffer + 1, size_t(star - buffer - 1)) != expected) return false;
    *star = 0;
    char *fields[16]; unsigned count = 1; fields[0] = buffer + 1;
    for (char *p = buffer + 1; *p; ++p) {
      if (*p == ',') {
        if (count == 16) return false;
        *p = 0; fields[count++] = p + 1;
      }
    }
    if (strcmp(fields[0], "MESP")) return false;
    const bool v1 = count == 14 && strcmp(fields[1], "1") == 0;
    const bool v2 = count == 16 && strcmp(fields[1], "2") == 0;
    if (!v1 && !v2) return false;
    strcpy(out.weatherStatus, "UNKNOWN");
    out.classificationAvailable = v2;
    if (v2) {
      uint32_t reasons;
      if (!integer(fields[15], 127, reasons)) return false;
      if (strcmp(fields[14], "UNKNOWN") && strcmp(fields[14], "NORMAL") &&
          strcmp(fields[14], "UNFAVORABLE") && strcmp(fields[14], "SEVERE")) return false;
      if (!strcmp(fields[14], "NORMAL") && reasons != 0) return false;
      if (!strcmp(fields[14], "UNKNOWN") && (reasons & 15)) return false;
      if ((!strcmp(fields[14], "UNFAVORABLE") || !strcmp(fields[14], "SEVERE")) &&
          (!reasons || (reasons & 112))) return false;
      if (!strcmp(fields[14], "SEVERE") && !(reasons & 3)) return false;
      strcpy(out.weatherStatus, fields[14]); out.weatherReasons = uint8_t(reasons);
    }
    uint32_t timeValid, flags;
    if (!integer(fields[2], UINT32_MAX, out.sequence) ||
        !integer(fields[3], UINT32_MAX, out.uptime) ||
        !integer(fields[5], 1, timeValid) || !integer(fields[13], 15, flags)) return false;
    out.flags = uint8_t(flags); out.timeValid = timeValid != 0;
    if (out.timeValid != bool(flags & 8)) return false;
    if (out.timeValid ? !timestamp(fields[4]) : *fields[4] != 0) return false;
    memcpy(out.timestamp, fields[4], strlen(fields[4]) + 1);
    const double minimum[] = {-40, 0, 300, 0, 0, 0, 0};
    const double maximum[] = {85, 100, 1100, 200000, 4095, 100, 1};
    for (unsigned i = 0; i < 7; ++i) {
      const char *s = fields[6 + i];
      unsigned bit = i < 3 ? 1 : i == 3 ? 2 : 4;
      if (!(flags & bit) && *s) return false;
      out.present[i] = *s != 0;
      if (!out.present[i]) continue;
      if (i < 4) { if (!decimal(s, out.values[i])) return false; }
      else {
        uint32_t v;
        if (!integer(s, uint32_t(maximum[i]), v)) return false;
        out.values[i] = v;
      }
      if (out.values[i] < minimum[i] || out.values[i] > maximum[i]) return false;
    }
    return true;
  }
};
