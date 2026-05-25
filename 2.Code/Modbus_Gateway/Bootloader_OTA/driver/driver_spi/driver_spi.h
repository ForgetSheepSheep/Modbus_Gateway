#ifndef __DRIVER_SPI_H
#define __DRIVER_SPI_H
#include "config.h"
/* SPI Flash∆¨—°øÿ÷∆ */
#define SPI_FLASH_CS_LOW()      gpio_bit_reset(GPIOE, GPIO_PIN_2)
#define SPI_FLASH_CS_HIGH()     gpio_bit_set(GPIOE, GPIO_PIN_2)
void DrvSPIInit(void);
uint8_t DrvSPIReadWriteByte(uint8_t tx_data);

#endif /* __DRIVER_SPI_H */
