#ifndef WEATHER_APP_H
#define WEATHER_APP_H

#include "stm32f1xx_hal.h"
#include "weather_data.h"

void WeatherApp_Init(I2C_HandleTypeDef *sensor_bus,
                     ADC_HandleTypeDef *rain_adc,
                     UART_HandleTypeDef *debug_uart);
void WeatherApp_Run(void);
const WeatherData *WeatherApp_GetLatestData(void);

#endif /* WEATHER_APP_H */
