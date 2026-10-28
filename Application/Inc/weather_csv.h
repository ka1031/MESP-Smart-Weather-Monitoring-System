#ifndef WEATHER_CSV_H
#define WEATHER_CSV_H
#include "weather_data.h"
#include <stddef.h>
extern const char WEATHER_CSV_HEADER[];
/* Returns bytes written excluding NUL, or zero on insufficient space. */
size_t WeatherCSV_Format(char *buffer, size_t capacity,
                         const WeatherData *data, uint32_t sample_tick);
#endif
