extern "C" {
#include "weather_classification.h"
#include "esp32_link.h"
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, const uint8_t *, uint16_t, uint32_t) { return HAL_OK; }
}
#include "../ESP32/MESP_ESP32/weather_receiver.h"
#include <assert.h>
#include <stdio.h>

int main() {
  WeatherClassifier state;
  WeatherClassifier_Init(&state);
  WeatherData data{};
  data.temperature_c=25; data.humidity_percent=60; data.pressure_hpa=1008;
  data.illuminance_lux=100; data.rain_raw=3000; data.rain_percent=25;
  data.valid_flags=7; // A missing RTC must not prevent classification.
  WeatherReceiver receiver;
  uint32_t tick=0, seq=0;
  auto send = [&]() {
    WeatherClassifier_Update(&state,&data,tick);
    char packet[320];
    size_t n=ESP32Link_Format(packet,sizeof(packet),&data,tick,seq++);
    assert(n);
    bool accepted=false;
    for (size_t i=0;i<n;++i) accepted=receiver.feed(packet[i],tick)||accepted;
    assert(accepted && receiver.sample.classificationAvailable);
    assert(!strcmp(receiver.sample.weatherStatus,WeatherStatus_Name(data.weather_status)));
    assert(receiver.sample.weatherReasons==data.weather_reasons);
    tick+=1000;
  };
  send(); assert(!strcmp(receiver.sample.weatherStatus,"UNKNOWN"));
  send(); send(); assert(!strcmp(receiver.sample.weatherStatus,"NORMAL"));
  data.rain_wet=1; data.humidity_percent=90;
  send(); send(); send(); assert(!strcmp(receiver.sample.weatherStatus,"UNFAVORABLE"));
  assert(receiver.sample.weatherReasons==12);
  data.temperature_c=40;
  send(); send(); send(); assert(!strcmp(receiver.sample.weatherStatus,"SEVERE"));
  assert(receiver.sample.weatherReasons==13);
  data.valid_flags=6;
  send(); assert(!strcmp(receiver.sample.weatherStatus,"UNKNOWN"));
  assert(receiver.sample.weatherReasons==16 && !receiver.sample.present[0]);
  data.valid_flags=7; data.temperature_c=25; data.humidity_percent=60; data.rain_wet=0;
  send(); send(); send(); assert(!strcmp(receiver.sample.weatherStatus,"NORMAL"));
  puts("STM32 classifier -> CSV/UART -> ESP32 receiver: status, reasons, missing data and recovery passed");
}
