# Hardware modules

## Implemented

- BME280: temperature, humidity and pressure
- BH1750: ambient light
- Rain module: analog plate wetness and digital wet/dry input
- DS3231: date and time
- SSD1306 OLED: local readings and SD status
- HW-125 microSD module: timestamped CSV logging
- ESP32 DevKit V1: UART receiver and local Wi-Fi dashboard

## Pending

- Green/red status LEDs and buzzer, with assigned pins and driver circuit
- Weather classification and alert handling
- Internet connectivity, cloud history and remote alerts
- Final enclosure, sensor mounting and system validation

PC13 is a heartbeat indicator, not a weather-status LED. Wind sensing has been
removed from scope. The OLED replaces the originally proposed 20×4 LCD.
