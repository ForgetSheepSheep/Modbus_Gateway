#ifndef __DRIVER_UART_H
#define __DRIVER_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void DrvUart2Init(uint32_t baudrate);
void DrvUart2SendByte(uint8_t data);
void DrvUart2SendString(const char *str);
uint8_t DrvUart2ReadByte(uint8_t *data);
void DrvUart2ClearRecvBuf(void);
uint8_t DrvUart2FindString(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_UART_H */
