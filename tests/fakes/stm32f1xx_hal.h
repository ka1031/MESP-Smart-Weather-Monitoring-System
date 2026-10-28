#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef struct {int unused;} I2C_HandleTypeDef;
typedef struct {int unused;} ADC_HandleTypeDef;
typedef struct {int unused;} GPIO_TypeDef;
typedef struct {struct {uint32_t BaudRatePrescaler;} Init; uint32_t ErrorCode;} SPI_HandleTypeDef;
typedef enum {GPIO_PIN_RESET, GPIO_PIN_SET} GPIO_PinState;
#define GPIO_PIN_4 16U
#define SPI_BAUDRATEPRESCALER_256 256U
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, uint8_t *, uint8_t *, uint16_t, uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
typedef struct {void *Instance; uint32_t ErrorCode;} UART_HandleTypeDef;
#define USART1 ((void *)1)
#define USART1_IRQn 37
#define HAL_UART_ERROR_NONE 0U
#define I2C_MEMADD_SIZE_8BIT 1U
#define GPIOA ((GPIO_TypeDef *)2)
#define GPIOC ((GPIO_TypeDef *)3)
#define GPIO_PIN_1 2U
#define GPIO_PIN_13 8192U
#define __HAL_UART_CLEAR_OREFLAG(u) ((void)(u))
static inline uint32_t __get_PRIMASK(void) {return 0U;}
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t v) {(void)v;}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
void HAL_NVIC_SetPriority(int, uint32_t, uint32_t);
void HAL_NVIC_EnableIRQ(int);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
void HAL_GPIO_TogglePin(GPIO_TypeDef *, uint16_t);
#endif
