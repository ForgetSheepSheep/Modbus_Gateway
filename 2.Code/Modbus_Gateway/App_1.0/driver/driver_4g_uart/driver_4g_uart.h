#ifndef __DRIVER_4G_UART_H
#define __DRIVER_4G_UART_H

#include "config.h"

void Drv4GUARTInit(uint32_t baudrate);
uint8_t Drv4GUARTSendString(char *str);
uint8_t Drv4GUARTReadByte(uint8_t *pdata);
uint32_t Drv4GUARTGetDropCount(void);
void Drv4GUARTIRQHandler(void);

#endif /* __DRIVER_4G_UART_H */
