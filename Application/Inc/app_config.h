#ifndef APP_CONFIG_H
#define APP_CONFIG_H

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

#define APP_CONFIG_BME280_ADDRESS_PRIMARY    (0x76U)
#define APP_CONFIG_BME280_ADDRESS_SECONDARY  (0x77U)
#define APP_CONFIG_BH1750_ADDRESS_PRIMARY    (0x23U)
#define APP_CONFIG_BH1750_ADDRESS_SECONDARY  (0x5CU)
#define APP_CONFIG_DS3231_ADDRESS            (0x68U)

/* Add rain calibration constants here after measuring the actual module. */

#endif /* APP_CONFIG_H */
