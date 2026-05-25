#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#include "config.h"
#include "mb.h"

typedef struct {
    eMBErrorCode (*ReadRegs)(uint16_t startAddr, uint16_t regNum, uint8_t *buf);
    eMBErrorCode (*WriteRegs)(uint16_t startAddr, uint16_t regNum, uint8_t *buf);
} ModbusFuncCb_t;

typedef struct {
    uint8_t slaveAddr;
    uint32_t baudRate;
    ModbusFuncCb_t cb;
} ModbusSlaveInstance_t;

void ModbusSlaveInit(ModbusSlaveInstance_t *mbInstance);

#endif /* __MODBUS_SLAVE_H */
