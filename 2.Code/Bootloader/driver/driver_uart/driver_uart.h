#ifndef __DRIVER_UART_H
#define __DRIVER_UART_H

#include "config.h"


/************************************************************
* @brief UART编号枚举
* @note  用于区分不同UART，目前只使用USART0
************************************************************/
typedef enum
{
    UART_ID_0 = 0,      /* USART0 */
    UART_ID_MAX

} UARTId_e;


/************************************************************
* @brief UART驱动初始化函数
* @param baudrate 串口波特率，例如115200
* @return 无
************************************************************/
void DrvUARTInit(uint32_t baudrate);


/************************************************************
* @brief UART发送一段数据
* @param uart_id 串口编号，目前USART0对应UART_ID_0
* @param pdata   待发送数据缓冲区指针
* @param len     待发送数据长度
* @return 无
************************************************************/
void DrvUARTSendData(uint8_t uart_id, uint8_t *pdata, uint16_t len);


/************************************************************
* @brief UART发送字符串
* @param uart_id 串口编号，目前USART0对应UART_ID_0
* @param str     待发送字符串指针
* @return 无
************************************************************/
void DrvUARTSendString(uint8_t uart_id, char *str);


/************************************************************
* @brief UART从RingBuffer读取1字节数据
* @param pdata 读取到的数据存放地址
* @return 1表示读取成功，0表示没有数据
************************************************************/
uint8_t DrvUARTReadByte(uint8_t *pdata);


#endif /* __DRIVER_UART_H */
