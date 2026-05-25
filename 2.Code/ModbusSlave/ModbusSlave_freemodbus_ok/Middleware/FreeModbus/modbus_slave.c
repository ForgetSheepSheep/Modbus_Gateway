#include "mb.h"
#include "mbutils.h"
#include "modbus_slave.h"


static ModbusFuncCb_t g_modbusFuncCb;

eMBErrorCode
eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode)
{
    eMBErrorCode state;
    /* 看什么模式，读还是写 */
    if(eMode == MB_REG_READ)
    {
        state = g_modbusFuncCb.ReadRegs(usAddress, usNRegs, pucRegBuffer);
    }
    else
    {
        state = g_modbusFuncCb.WriteRegs(usAddress, usNRegs, pucRegBuffer);
    }
    return state;
}


eMBErrorCode
eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
    eMBErrorCode state;
    state = g_modbusFuncCb.ReadRegs(usAddress, usNRegs, pucRegBuffer);
    return state;
}


eMBErrorCode
eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode)
{
    eMBErrorCode state;
    if(eMode == MB_REG_READ)
    {
        state = g_modbusFuncCb.ReadRegs(usAddress, usNCoils, pucRegBuffer);
    }
    else
    {
         state = g_modbusFuncCb.WriteRegs(usAddress, usNCoils, pucRegBuffer);
    }
    return state;
}

eMBErrorCode
eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
    
    eMBErrorCode state;
    state = g_modbusFuncCb.ReadRegs(usAddress, usNDiscrete, pucRegBuffer);
    return state;
}
void ModbusSlaveInit(ModbusSlaveInstance_t *mbInstance)
{
    eMBInit(MB_RTU, mbInstance->slaveAddr, 0, mbInstance->baudRate, MB_PAR_NONE);
    g_modbusFuncCb = mbInstance->cb;
    eMBEnable();
}
