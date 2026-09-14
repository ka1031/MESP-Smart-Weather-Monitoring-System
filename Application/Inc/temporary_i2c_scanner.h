#ifndef TEMPORARY_I2C_SCANNER_H
#define TEMPORARY_I2C_SCANNER_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

/*
 * TEMPORARY HARDWARE DEBUG UTILITY
 *
 * Remove the call to TemporaryI2CScanner_Run() after the I2C wiring has been
 * verified. The scanner performs a blocking probe of the complete usable
 * 7-bit address range and is not intended for the final application loop.
 */
uint8_t TemporaryI2CScanner_Run(I2C_HandleTypeDef *i2c);

#endif /* TEMPORARY_I2C_SCANNER_H */
