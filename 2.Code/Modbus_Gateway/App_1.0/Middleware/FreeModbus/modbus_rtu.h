#ifndef _MODBUS_RTU_H_
#define _MODBUS_RTU_H_

#include "port_macro.h"

#define MB_FUNC_NONE                          (  0 )
#define MB_FUNC_READ_COILS                    (  1 )
#define MB_FUNC_READ_DISCRETE_INPUTS          (  2 )
#define MB_FUNC_WRITE_SINGLE_COIL             (  5 )
#define MB_FUNC_WRITE_MULTIPLE_COILS          ( 15 )
#define MB_FUNC_READ_HOLDING_REGISTER         (  3 )
#define MB_FUNC_READ_INPUT_REGISTER           (  4 )
#define MB_FUNC_WRITE_SINGLE_REGISTER         (  6 )
#define MB_FUNC_WRITE_MULTIPLE_REGISTERS      ( 16 )
#define MB_FUNC_READWRITE_MULTIPLE_REGISTERS  ( 23 )
#define MB_FUNC_DIAG_READ_EXCEPTION           (  7 )
#define MB_FUNC_DIAG_DIAGNOSTIC               (  8 )
#define MB_FUNC_DIAG_GET_COM_EVENT_CNT        ( 11 )
#define MB_FUNC_DIAG_GET_COM_EVENT_LOG        ( 12 )
#define MB_FUNC_OTHER_REPORT_SLAVEID          ( 17 )
#define MB_FUNC_ERROR                         ( 128 )

#define MODBUS_RTU_ADU_MAX_SIZE	               256
#define MODBUS_RTU_ADU_MIN_SIZE	               4


typedef enum
{
	MB_STATE_IDLE,
	MB_STATE_TX_ING,
	MB_STATE_TX_END,
	MB_STATE_RX_WAIT,
	MB_STATE_RX_ING,
	MB_STATE_RX_CHECK,
	MB_STATE_RX_ERR,		// 接收到的响应错误
	MB_STATE_RX_TIMEOUT, 	// 接收响应超时
	MB_STATE_RX_SUCESS 
} Modbus_Master_state_t;

void ModbusMasterRTUInit(UCHAR port, ULONG baudRate, UCHAR parity);

/**
 ******************************************************************************
 * @brief 0x03指令 Read Holding Registers
 * @param slaveAddr: 从机地址
 * @param regAddr: 寄存器地址
 * @param regQuantity: 寄存器个数
 ******************************************************************************
 */
void ModbusMasterReadHoldingRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regQuantity);

/**
 ******************************************************************************
 * @brief 0x06指令 Write Single Holding Register
 * @param slaveAddr: 从机地址
 * @param regAddr: 寄存器地址
 * @param regVal: 寄存器数据
 ******************************************************************************
 */
void ModbusMasterWriteSingleRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regVal);

/**
 ******************************************************************************
 * @brief 0x10指令 Write Multiple Holding Register
 * @param slaveAddr: 从机地址
 * @param regAddr: 寄存器地址
 * @param regQuantity: 寄存器个数
 * @param regVal: 寄存器数据数组
 ******************************************************************************
 */
void ModbusMasterWriteMultipleRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regQuantity, USHORT *regVal);

/**
 **********************************************************************************************
 * @brief 提供给应用获取modbus主机当前运行状态，在函数内部会将接收后的状态全部设置为空闲
 * @return 返回状态
 **********************************************************************************************
 */
Modbus_Master_state_t ModbusMasterGetRequestState(void);

/**
 **********************************************************************************************
 * @brief 提供给应用获取modbus主机接收到的数据
 * @param aduFrame: 完整的一帧（ADU）数据，包括从机地址
 * @return 返回接收到的一帧数据长度
 **********************************************************************************************
 */
USHORT ModbusMasterGetRequestFrame(UCHAR *aduFrame);

/**
 **********************************************************************************************
 * @brief modbus主机主流程处理函数，需要在主流程中及时的被循环调用
 **********************************************************************************************
 */
void ModbusMasterPoll(void);
void ModbusMasterTimerExpiredISR(void);
void ModbusMasterRxISR(void);
#endif
