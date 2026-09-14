**STM32F103C8Tx Pin Configuration (image_101d70.jpg)**

| Pin Name | Signal / Function | Peripheral | Description |
| --- | --- | --- | --- |
| **PC13** | GPIO_Output | GPIO | Active-low Blue Pill status LED / heartbeat |
| **PA0** | ADC1_IN0 | ADC1 | Rain module analog output (AO) |
| **PA1** | GPIO_Input | GPIO | Rain module digital output (DO), normally active-low |
| **PA2** | USART2_TX | USART2 | Universal Synchronous/Asynchronous Receiver Transmitter 2 (Transmit) |
| **PA3** | USART2_RX | USART2 | Universal Synchronous/Asynchronous Receiver Transmitter 2 (Receive) |
| **PA9** | USART1_TX | USART1 | Universal Synchronous/Asynchronous Receiver Transmitter 1 (Transmit) |
| **PA10** | USART1_RX | USART1 | Universal Synchronous/Asynchronous Receiver Transmitter 1 (Receive) |
| **PA13** | SYS_JTMS-SWDIO | SYS | Serial Wire Debug Data I/O (SWDIO) |
| **PA14** | SYS_JTCK-SWCLK | SYS | Serial Wire Debug Clock (SWCLK) |
| **PB8** | I2C1_SCL | I2C1 | Shared BME280, BH1750, and SSD1306 clock |
| **PB9** | I2C1_SDA | I2C1 | Shared BME280, BH1750, and SSD1306 data |
| **PD0** | RCC_OSC_IN | RCC | High-speed external clock input |
| **PD1** | RCC_OSC_OUT | RCC | High-speed external clock output |
