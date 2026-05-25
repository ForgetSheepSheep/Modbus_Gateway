#include <string.h>
#include <stdio.h>
#include "modbus_rtu.h"
#include "modbus_crc.h"
#include "port_serial.h"
#include "port_timer.h"

typedef struct 
{
    Modbus_Master_state_t state;                // modbus master??
    UCHAR errTimes;                   			// ??????????????
    USHORT txLen;                      			// ??????????????????
    USHORT txCounter;                  			// ???????bytes??????
    UCHAR txBuf[MODBUS_RTU_ADU_MAX_SIZE];       // ?????????????
    USHORT rxCounter;                  			// ??????????
    UCHAR rxBuf[MODBUS_RTU_ADU_MAX_SIZE];       // ?????????????
    ULONG rxTimeout;                  		    // ???????????????????3.5T
} Modbus_Master_t;


static Modbus_Master_t g_modbusMaster;

void ModbusMasterRTUInit(UCHAR port, ULONG baudRate, UCHAR parity)
{
    ULONG t35TimesOf50us;

    /* Modbus RTU uses 8 Databits. */
    ModbusMasterSerialInit(port, baudRate, 8, parity);

	/* If baudrate > 19200 then we should use the fixed timer values
	 * t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
	 */
	if( baudRate > 19200 )
	{
		t35TimesOf50us = 35;       /* 1800us. */
	}
	else
	{
		/* The timer reload value for a character is given by:
		 *
		 * ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
		 *             = 11 * Ticks_per_1s / Baudrate
		 *             = 220000 / Baudrate
		 * The reload for t3.5 is 1.5 times this value and similary
		 * for t3.5.
		 */
		t35TimesOf50us = (7UL * 220000UL) / (2UL * baudRate);
	}
	ModbusMasterPortTimerInit((USHORT)t35TimesOf50us);
	ModbusMasterPortSerialEnable(TRUE, FALSE);
}

/**
 **********************************************************************************************
 * @brief ????????????modbus???????????????????????????????????????????????????????????????
 * @return ????????
 **********************************************************************************************
 */
Modbus_Master_state_t ModbusMasterGetRequestState(void)
{
	Modbus_Master_state_t state = g_modbusMaster.state; 
	if (g_modbusMaster.state == MB_STATE_RX_SUCESS || g_modbusMaster.state == MB_STATE_RX_ERR || g_modbusMaster.state == MB_STATE_RX_TIMEOUT)
	{
		g_modbusMaster.state = MB_STATE_IDLE;
	}
	return state;
}

/**
 **********************************************************************************************
 * @brief ????????????modbus??????????????????????
 * @param aduFrame: ??????????????ADU????????????????????????
 * @return ??????????????????????????
 **********************************************************************************************
 */
USHORT ModbusMasterGetRequestFrame(UCHAR *aduFrame)
{
	USHORT frameLen;
	AssertParam(aduFrame != NULL);
	
	frameLen = g_modbusMaster.rxCounter;
	memcpy(aduFrame, g_modbusMaster.rxBuf, g_modbusMaster.rxCounter);
	g_modbusMaster.rxCounter = 0;
	return frameLen;
}

/**
 ******************************************************************************
 * @brief 0x03???? Read Holding Registers
 * @param slaveAddr: ????????
 * @param regAddr: ???????????
 * @param regQuantity: ?????????????
 ******************************************************************************
 */
void ModbusMasterReadHoldingRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regQuantity)
{
	USHORT crc;
	
	if (g_modbusMaster.state != MB_STATE_IDLE)
	{
		return;
	}
	g_modbusMaster.txCounter = 0;
    g_modbusMaster.rxCounter = 0;
    g_modbusMaster.txBuf[0] = slaveAddr;
    g_modbusMaster.txBuf[1] = MB_FUNC_READ_HOLDING_REGISTER;
    g_modbusMaster.txBuf[2] = regAddr >> 8;
    g_modbusMaster.txBuf[3] = regAddr;
    g_modbusMaster.txBuf[4] = regQuantity >> 8;
    g_modbusMaster.txBuf[5] = regQuantity;
	
	crc = ModbusCRC16(g_modbusMaster.txBuf, 6);
	g_modbusMaster.txBuf[6] = (UCHAR)(crc & 0xff);
    g_modbusMaster.txBuf[7] = (UCHAR)(crc >> 8);
	
	g_modbusMaster.txLen = 8;
	
	ModbusMasterSendFrame(g_modbusMaster.txBuf, g_modbusMaster.txLen);
	g_modbusMaster.state = MB_STATE_TX_END;
}

/**
 ******************************************************************************
 * @brief 0x06???? Write Single Holding Register
 * @param slaveAddr: ????????
 * @param regAddr: ???????????
 * @param regVal: ?????????????
 ******************************************************************************
 */
void ModbusMasterWriteSingleRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regVal)
{
	USHORT crc;
	
	if (g_modbusMaster.state != MB_STATE_IDLE)
	{
		return;
	}
	g_modbusMaster.txCounter = 0;
    g_modbusMaster.rxCounter = 0;
    g_modbusMaster.txBuf[0] = slaveAddr;
    g_modbusMaster.txBuf[1] = MB_FUNC_WRITE_SINGLE_REGISTER;
    g_modbusMaster.txBuf[2] = regAddr >> 8;
    g_modbusMaster.txBuf[3] = regAddr;
    g_modbusMaster.txBuf[4] = regVal >> 8;
    g_modbusMaster.txBuf[5] = regVal;
	
	crc = ModbusCRC16(g_modbusMaster.txBuf, 6);
	g_modbusMaster.txBuf[6] = (UCHAR)(crc & 0xff);
    g_modbusMaster.txBuf[7] = (UCHAR)(crc >> 8);
	
	g_modbusMaster.txLen = 8;
	
	ModbusMasterSendFrame(g_modbusMaster.txBuf, g_modbusMaster.txLen);
	g_modbusMaster.state = MB_STATE_TX_END;
}

/**
 ******************************************************************************
 * @brief 0x10???? Write Multiple Holding Register
 * @param slaveAddr: ????????
 * @param regAddr: ???????????
 * @param regQuantity: ?????????????
 * @param regVal: ???????????????????
 ******************************************************************************
 */
void ModbusMasterWriteMultipleRegRequest(UCHAR slaveAddr, USHORT regAddr, USHORT regQuantity, USHORT *regVal)
{
	USHORT crc;
	USHORT i;
	
	if (g_modbusMaster.state != MB_STATE_IDLE)
	{
		return;
	}
	g_modbusMaster.txCounter = 0;
    g_modbusMaster.rxCounter = 0;
    g_modbusMaster.txBuf[0] = slaveAddr;
    g_modbusMaster.txBuf[1] = MB_FUNC_WRITE_MULTIPLE_REGISTERS;
    g_modbusMaster.txBuf[2] = regAddr >> 8;
    g_modbusMaster.txBuf[3] = regAddr;
    g_modbusMaster.txBuf[4] = regQuantity >> 8;
    g_modbusMaster.txBuf[5] = regQuantity;
	g_modbusMaster.txBuf[6] = regQuantity * 2;
	
	for (i = 0; i < regQuantity; i++)
	{
		g_modbusMaster.txBuf[7 + i * 2] = regVal[i] >> 8;
		g_modbusMaster.txBuf[8 + i * 2] = regVal[i];
	}
	
	g_modbusMaster.txLen = 7 + regQuantity * 2;
	crc = ModbusCRC16(g_modbusMaster.txBuf, g_modbusMaster.txLen);
	g_modbusMaster.txBuf[7 + regQuantity * 2] = (UCHAR)(crc & 0xff);
    g_modbusMaster.txBuf[8 + regQuantity * 2] = (UCHAR)(crc >> 8);
	
	g_modbusMaster.txLen = g_modbusMaster.txLen + 2;
	
	ModbusMasterSendFrame(g_modbusMaster.txBuf, g_modbusMaster.txLen);
	g_modbusMaster.state = MB_STATE_TX_END;
}

/**
 **********************************************************************************************
 * @brief modbus??????????????????????????????????
 **********************************************************************************************
 */
void ModbusMasterPoll(void)
{
	switch (g_modbusMaster.state)
	{
		case MB_STATE_IDLE:
			ModbusMasterPortSerialEnable(TRUE, FALSE);
			break;
		case MB_STATE_TX_END:
			ModbusMasterPortTimerEnable();
			g_modbusMaster.rxTimeout = 0;
			ModbusMasterPortSerialEnable(TRUE, FALSE);
			g_modbusMaster.state = MB_STATE_RX_WAIT;
			break;

        /* Check one received response frame. */
		case MB_STATE_RX_CHECK:
			if ((g_modbusMaster.rxCounter >= MODBUS_RTU_ADU_MIN_SIZE) && (ModbusCRC16(g_modbusMaster.rxBuf, g_modbusMaster.rxCounter) == 0))
			{
				if ((g_modbusMaster.txBuf[0] == g_modbusMaster.rxBuf[0]) && (g_modbusMaster.txBuf[1] == g_modbusMaster.rxBuf[1]))
				{
					g_modbusMaster.state = MB_STATE_RX_SUCESS;
				}
				else
				{
					g_modbusMaster.state = MB_STATE_RX_ERR;
				}
			}
			else
			{
				g_modbusMaster.state = MB_STATE_RX_ERR;
			}
			break;
		default:
			break;
	}
}

/**
 **********************************************************************************************
 * @brief ????????????????????????????????????????????????
 **********************************************************************************************
 */
void ModbusMasterTimerExpiredISR(void)
{
	switch (g_modbusMaster.state)
	{
		case MB_STATE_RX_WAIT:
			g_modbusMaster.rxTimeout++;
			if (g_modbusMaster.rxTimeout >= MODBUS_RX_MAX_TIMEOUT) // ????????
			{
                g_modbusMaster.rxTimeout = 0;
				g_modbusMaster.state = MB_STATE_RX_TIMEOUT;       
				ModbusMasterPortTimerDisable();          // ?????????
			}
			break;
		case MB_STATE_RX_ING:      //3.5T???,?????????????
			g_modbusMaster.state = MB_STATE_RX_CHECK;
			ModbusMasterPortTimerDisable();     //?????????
			break;
		default:
			break;
    }
}

/**
 **********************************************************************************************
 * @brief ??????????????????????????????????????????????????????????
 **********************************************************************************************
 */
void ModbusMasterRxISR(void)
{
    CHAR ch;
    ModbusMasterPortSerialGetByte(&ch);

    switch (g_modbusMaster.state)
    {
		case MB_STATE_RX_WAIT:
			g_modbusMaster.rxCounter = 0;
			g_modbusMaster.rxBuf[g_modbusMaster.rxCounter++] = ch;
			g_modbusMaster.state = MB_STATE_RX_ING;
			ModbusMasterPortTimerEnable();
			break;
    case MB_STATE_RX_ING:
        if (g_modbusMaster.rxCounter <= MODBUS_RTU_ADU_MAX_SIZE - 1)
        {
            g_modbusMaster.rxBuf[g_modbusMaster.rxCounter++] = ch;
        }
        ModbusMasterPortTimerEnable();
        break;
    default:
        break;
    }
}
