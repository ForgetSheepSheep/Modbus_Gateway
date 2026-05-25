#ifndef __DRIVER_UART_H
#define __DRIVER_UART_H

#include "config.h"

/************************************************************
* @brief 串口编号枚举
* @note  用于区分调试串口和LoRa串口
************************************************************/
typedef enum
{
    UART_ID_DEBUG = 0,      /* 调试串口，对应USART0 */
    UART_ID_4G  = 1,        /* 4G串口，对应USART2 */

    UART_ID_MAX

} UartId_t;


void DrvUARTInit(uint32_t baudrate);
void DrvUARTSendData(uint8_t uart_id, uint8_t *pdata, uint16_t len);
void DrvUARTSendString(uint8_t uart_id, char *str);
uint8_t DrvUARTReadByte(uint8_t uart_id, uint8_t *pdata);

#endif
