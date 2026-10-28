#ifndef SD_LOGGER_H
#define SD_LOGGER_H
#include "stm32f1xx_hal.h"
#include "weather_data.h"
#include <stddef.h>

typedef enum { SD_LOGGER_STOPPED, SD_LOGGER_PENDING, SD_LOGGER_ACTIVE,
               SD_LOGGER_ERROR } SDLogger_State;
/* Init and START defer mounting until the next sample supplies its RTC time. */
void SDLogger_Init(SPI_HandleTypeDef *spi, GPIO_TypeDef *port, uint16_t pin);
void SDLogger_Start(void);
void SDLogger_Stop(void);
void SDLogger_Process(const WeatherData *data, uint32_t sample_tick);
void SDLogger_Report(void);
void SDLogger_DisplayText(char *buffer, size_t capacity);
SDLogger_State SDLogger_GetState(void);
#endif
