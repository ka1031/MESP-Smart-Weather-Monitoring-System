#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "weather_receiver.h"

// ESP32-WROOM-32 / 30-pin DevKit V1. No external Arduino libraries needed.
static const char *AP_NAME = "MESP-Weather";
static const char *AP_PASSWORD = "MespWeather32";
HardwareSerial weatherUart(2);
WebServer server(80);
WeatherReceiver receiver;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MESP Weather Station</title><style>
body{font:18px system-ui;background:#eef4f6;color:#182c38;max-width:760px;margin:32px auto;padding:20px}
h1{font-size:30px}#status{padding:16px;background:#fff;border-radius:12px}
dl{display:grid;grid-template-columns:1fr 1fr;gap:16px;background:white;padding:24px;border-radius:12px}
dd{margin:0;font-weight:600}small{color:#465b68}</style>
<h1>MESP Weather Station</h1><p id="status">Connecting…</p><p id="time"></p><dl id="readings"></dl>
<small>Rain percentage measures sensor plate wetness, not rainfall depth. Weather classification is not enabled.</small>
<script>
const labels=['Temperature','Humidity','Pressure','Light','Rain ADC','Plate wetness','Rain detected'];
const units=[' °C',' %',' hPa',' lux','',' %',''];
async function refresh(){try{const r=await fetch('/api/readings',{cache:'no-store'});if(!r.ok)throw Error();const d=await r.json();
document.getElementById('status').textContent=!d.has_sample?'Waiting for STM32':d.fresh?'Live • valid packets '+d.accepted+' • rejected '+d.rejected:'STALE • no valid packet for '+Math.floor(d.age_ms/1000)+' seconds';
document.getElementById('time').textContent=d.timestamp?'Last sample: '+d.timestamp+' (RTC local time)':'RTC time unavailable';
const list=document.getElementById('readings');list.replaceChildren();labels.forEach((name,i)=>{const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=name;const v=d.values[i];dd.textContent=v===null?'Unavailable':i===6?(v?'Wet':'Dry'):v+units[i];list.append(dt,dd)});
}catch(e){document.getElementById('status').textContent='Disconnected from ESP32 — readings below may be old';}
setTimeout(refresh,1000);}refresh();</script></html>)HTML";

static void sendReadings() {
  uint32_t now = millis();
  String json;
  json.reserve(500);
  json = "{\"has_sample\":"; json += receiver.hasSample ? "true" : "false";
  json += ",\"fresh\":"; json += receiver.fresh(now) ? "true" : "false";
  json += ",\"age_ms\":"; json += receiver.hasSample ? String(uint32_t(now - receiver.lastReceived)) : "null";
  json += ",\"accepted\":"; json += String(receiver.accepted);
  json += ",\"rejected\":"; json += String(receiver.rejected);
  json += ",\"sequence\":"; json += String(receiver.sample.sequence);
  json += ",\"uptime_ms\":"; json += String(receiver.sample.uptime);
  json += ",\"valid_flags\":"; json += String(receiver.sample.flags);
  json += ",\"timestamp\":";
  if (receiver.hasSample && receiver.sample.timeValid) {
    json += '"'; json += receiver.sample.timestamp; json += '"';
  } else json += "null";
  json += ",\"values\":[";
  for (unsigned i = 0; i < 7; ++i) {
    if (i) json += ',';
    json += receiver.hasSample && receiver.sample.present[i] ? String(receiver.sample.values[i], 2) : "null";
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  weatherUart.setRxBufferSize(2048);
  weatherUart.begin(115200, SERIAL_8N1, 16, 17);
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(AP_NAME, AP_PASSWORD)) {
    Serial.printf("\nMESP receiver ready. Wi-Fi: %s\nPassword: %s\n", AP_NAME, AP_PASSWORD);
    Serial.print("Open http://"); Serial.println(WiFi.softAPIP());
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
    server.on("/api/readings", HTTP_GET, sendReadings);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
    server.begin();
  } else Serial.println("Wi-Fi AP failed; UART receiver remains active.");
  Serial.println("Waiting for STM32 on GPIO16 at 115200 baud...");
}

void loop() {
  // Limit each batch so noise on UART cannot starve the web server.
  for (unsigned i = 0; i < 2048 && weatherUart.available(); ++i) {
    if (receiver.feed(char(weatherUart.read()), millis())) {
      Serial.printf("RX OK seq=%lu RTC=%s flags=%u\n",
        (unsigned long)receiver.sample.sequence,
        receiver.sample.timeValid ? receiver.sample.timestamp : "unavailable", receiver.sample.flags);
      Serial.print("T,H,P,L,rain_raw,rain_pct,rain_wet: ");
      for (unsigned j = 0; j < 7; ++j) {
        if (j) Serial.print(',');
        if (receiver.sample.present[j]) Serial.print(receiver.sample.values[j], 2);
        else Serial.print("NA");
      }
      Serial.println();
    }
  }
  receiver.expire(millis());
  server.handleClient();
  static uint32_t lastStatus = 0;
  uint32_t now = millis();
  if (uint32_t(now - lastStatus) >= 5000) {
    lastStatus = now;
    Serial.printf("Link: %s | accepted=%lu rejected=%lu\n",
      receiver.fresh(now) ? "LIVE" : receiver.hasSample ? "STALE" : "WAITING",
      (unsigned long)receiver.accepted, (unsigned long)receiver.rejected);
  }
  delay(1);
}
