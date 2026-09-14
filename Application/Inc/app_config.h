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
#define APP_CONFIG_I2C_TIMEOUT_MS            (100U)
#define APP_CONFIG_UART_TIMEOUT_MS           (100U)

/* Prototype feature selection. */
#define APP_CONFIG_ENABLE_BME280              (1U)
#define APP_CONFIG_ENABLE_BH1750              (1U)
#define APP_CONFIG_ENABLE_RAIN_SENSOR         (1U)
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
