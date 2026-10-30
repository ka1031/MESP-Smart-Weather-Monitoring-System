#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f1xx_hal.h"

/*
 * Application-level configuration.
 *
 * I2C addresses in this file are unshifted 7-bit addresses. Device drivers
 * are responsible for converting them to the format expected by STM32 HAL.
 * Pin assignments remain owned by STM32CubeMX and Core/Inc/main.h.
 */

#define APP_CONFIG_SAMPLE_PERIOD_MS          (1000U)
/* Prototype condition thresholds, not official weather-warning criteria.
 * Pressure and light remain informational; rain uses the digital wet input. */
#define APP_CONFIG_WEATHER_HOT_WARN_C         (35.0F)
#define APP_CONFIG_WEATHER_HOT_SEVERE_C       (40.0F)
#define APP_CONFIG_WEATHER_COLD_WARN_C        (10.0F)
#define APP_CONFIG_WEATHER_COLD_SEVERE_C      (5.0F)
#define APP_CONFIG_WEATHER_TEMP_HYST_C        (1.0F)
#define APP_CONFIG_WEATHER_HUMID_WARN_PCT     (85.0F)
#define APP_CONFIG_WEATHER_HUMID_HYST_PCT     (5.0F)
#define APP_CONFIG_WEATHER_CONFIRM_SAMPLES   (3U)
#define APP_CONFIG_WEATHER_CLEAR_SAMPLES     (10U)
#define APP_CONFIG_WEATHER_MAX_GAP_MS        (5000U)
#define APP_CONFIG_I2C_TIMEOUT_MS            (100U)
#define APP_CONFIG_UART_TIMEOUT_MS           (100U)

#ifndef APP_CONFIG_ENABLE_ESP32_LINK
#define APP_CONFIG_ENABLE_ESP32_LINK           (1U)
#endif
/* USART2: 115200 8N1. A full 319-byte packet takes under 28 ms on the wire. */
#define APP_CONFIG_ESP32_UART_TIMEOUT_MS       (50U)

/* Prototype feature selection. */
#ifndef APP_CONFIG_ENABLE_SD_DIAGNOSTIC
#define APP_CONFIG_ENABLE_SD_DIAGNOSTIC       (0U)
#endif
#ifndef APP_CONFIG_ENABLE_SD_LOGGING
#define APP_CONFIG_ENABLE_SD_LOGGING          (1U)
#endif
#define APP_CONFIG_SD_LOG_PERIOD_MS           (5000U)
#if APP_CONFIG_ENABLE_SD_LOGGING && APP_CONFIG_ENABLE_SD_DIAGNOSTIC
#error "Select SD logging or the standalone read-only diagnostic, not both"
#endif
#define APP_CONFIG_ENABLE_BME280              (1U)
#define APP_CONFIG_ENABLE_BH1750              (1U)
#define APP_CONFIG_ENABLE_RAIN_SENSOR         (1U)
#define APP_CONFIG_ENABLE_DS3231              (1U)
/* Seed an unset/invalid RTC once per boot. Preserve a valid running clock.
 * Fixed local time (IST); this is a fallback, not live time synchronization. */
#ifndef APP_CONFIG_RTC_AUTO_SET
#define APP_CONFIG_RTC_AUTO_SET               (1U)
#endif
#define APP_CONFIG_RTC_START_YEAR             (2026U)
#define APP_CONFIG_RTC_START_MONTH            (10U)
#define APP_CONFIG_RTC_START_DAY              (31U)
#define APP_CONFIG_RTC_START_HOUR             (9U)
#define APP_CONFIG_RTC_START_MINUTE           (20U)
#define APP_CONFIG_RTC_START_SECOND           (0U)
#define APP_CONFIG_ENABLE_OLED                (1U)

/* Keep the scanner available, but disabled during normal measurements. */
#define APP_CONFIG_ENABLE_I2C_SCANNER         (0U)

#define APP_CONFIG_BME280_ADDRESS_PRIMARY    (0x76U)
#define APP_CONFIG_BME280_ADDRESS_SECONDARY  (0x77U)
#define APP_CONFIG_BH1750_ADDRESS_PRIMARY    (0x23U)
#define APP_CONFIG_BH1750_ADDRESS_SECONDARY  (0x5CU)
#define APP_CONFIG_DS3231_ADDRESS            (0x68U)

#define APP_CONFIG_OLED_ADDRESS_PRIMARY      (0x3CU)
#define APP_CONFIG_OLED_ADDRESS_SECONDARY    (0x3DU)

/* Rain module connections configured by CubeMX. */
#define APP_CONFIG_RAIN_DIGITAL_PORT          GPIOA
#define APP_CONFIG_RAIN_DIGITAL_PIN           GPIO_PIN_1
#define APP_CONFIG_RAIN_DIGITAL_ACTIVE_LOW    (1U)
#define APP_CONFIG_RAIN_ADC_SAMPLE_COUNT      (16U)
#define APP_CONFIG_RAIN_ADC_TIMEOUT_MS        (10U)

/*
 * Default analog calibration: most LM393 rain modules read high when dry and
 * low when wet. Replace these endpoints with values measured on your module.
 */
#define APP_CONFIG_RAIN_DRY_RAW               (4095U)
#define APP_CONFIG_RAIN_WET_RAW               (0U)

#endif /* APP_CONFIG_H */
