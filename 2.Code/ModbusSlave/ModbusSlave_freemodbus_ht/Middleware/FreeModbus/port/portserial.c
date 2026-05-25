/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- STM32 includes   ---------------------------------*/
#include "stm32f0xx_hal.h"
#include "main.h"

/* Modbus 使用 USART1 (PA9/PA10), RS485控制引脚 PA8 */
static UART_HandleTypeDef huart1;

/* LED调试: LED1=PB11, LED2=PB12, LED3=PB13 */
#define DBG_LED1_ON   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET)
#define DBG_LED1_OFF  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET)
#define DBG_LED2_ON   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define DBG_LED2_OFF  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)
#define DBG_LED3_ON   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET)
#define DBG_LED3_OFF  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET)

/* RS485 方向控制: PA8 */
#define RS485_DIR_PIN    GPIO_PIN_8
#define RS485_DIR_PORT   GPIOA
#define RS485_TX_MODE    HAL_GPIO_WritePin(RS485_DIR_PORT, RS485_DIR_PIN, GPIO_PIN_SET)    /* DE高=发送 */
#define RS485_RX_MODE    HAL_GPIO_WritePin(RS485_DIR_PORT, RS485_DIR_PIN, GPIO_PIN_RESET)  /* DE低=接收 */

#define UART_IT_RX_ENABLE   __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE)
#define UART_IT_RX_DISABLE  __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE)

#define UART_IT_TX_ENABLE   __HAL_UART_ENABLE_IT(&huart1, UART_IT_TXE)
#define UART_IT_TX_DISABLE  __HAL_UART_DISABLE_IT(&huart1, UART_IT_TXE)


/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );


/* -----------------------      STM32 CODE      -----------------------------*/
static BOOL SerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    /* 先关掉所有调试灯 */
    DBG_LED1_OFF;
    DBG_LED2_OFF;
    DBG_LED3_OFF;

    /* 配置 RS485 方向控制引脚 PA8 (GPIO配置由 HAL_UART_MspInit 处理) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = RS485_DIR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RS485_DIR_PORT, &GPIO_InitStruct);
    RS485_RX_MODE;  /* 默认接收模式 */

    huart1.Instance = USART1;
    huart1.Init.BaudRate = ulBaudRate;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    switch(eParity)
    {
        case MB_PAR_ODD:
            huart1.Init.Parity = UART_PARITY_ODD;
            huart1.Init.WordLength = UART_WORDLENGTH_9B;
            break;
        case MB_PAR_EVEN:
            huart1.Init.Parity = UART_PARITY_EVEN;
            huart1.Init.WordLength = UART_WORDLENGTH_9B;
            break;
        default:
            huart1.Init.Parity = UART_PARITY_NONE;
            huart1.Init.WordLength = UART_WORDLENGTH_8B;
            break;
    }
    if(HAL_UART_Init(&huart1) != HAL_OK)
    {
        DBG_LED1_ON;  /* 初始化失败，LED1常亮 */
        return FALSE;
    }

    /* 使能 USART1 中断 */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    DBG_LED1_ON;  /* 初始化成功，LED1亮 */
    return TRUE;
}

/* ----------------------- Start implementation -----------------------------*/
void
vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
    if(xRxEnable == TRUE)
    {
        UART_IT_RX_ENABLE;
    }
    else
    {
        UART_IT_RX_DISABLE;
    }

    if(xTxEnable == TRUE)
    {
        RS485_TX_MODE;  /* 开始发送时切换到发送模式 */
        UART_IT_TX_ENABLE;
    }
    else
    {
        UART_IT_TX_DISABLE;
        /* RS485 方向由 TC 中断在最后一个字节发完后切换，此处不操作 */
    }
}

BOOL
xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
    return SerialInit(ucPORT, ulBaudRate, ucDataBits, eParity);
}

BOOL
xMBPortSerialPutByte( CHAR ucByte )
{
    USART1->TDR = ucByte;
    return TRUE;
}

BOOL
xMBPortSerialGetByte( CHAR * pucByte )
{
    *pucByte = ((uint16_t)0x00ff & USART1->RDR);
    return TRUE;
}

static void prvvUARTTxReadyISR( void )
{
    pxMBFrameCBTransmitterEmpty(  );
}

static void prvvUARTRxISR( void )
{
    pxMBFrameCBByteReceived(  );
}

/* USART1 中断处理 */
void USART1_IRQHandler(void)
{
    if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE))
    {
        prvvUARTRxISR();  /* xMBPortSerialGetByte() 读 RDR 会自动清除 RXNE */
    }
    if(__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_TXE) &&
       __HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE))
    {
        /* 调用 FreeModbus TX FSM：写 TDR 清除 TXE，或禁用 TXE（发送完毕） */
        prvvUARTTxReadyISR();
        /* 如果 TXE 已被 FSM 禁用（最后一字节已写入 TDR），
         * 启用 TC 中断，等移位寄存器发完后切换 RS485 */
        if(!__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_TXE))
        {
            __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_TC);
            __HAL_UART_ENABLE_IT(&huart1, UART_IT_TC);
        }
    }
    if(__HAL_UART_GET_IT_SOURCE(&huart1, UART_IT_TC) &&
       __HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC))
    {
        __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_TC);
        __HAL_UART_DISABLE_IT(&huart1, UART_IT_TC);
        RS485_RX_MODE;  /* 最后一个字节已从移位寄存器发出，安全切换到接收 */
    }
}
