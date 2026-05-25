#include "app_modbusslave.h"
#include "modbus_slave.h"
#include "./driver_io/driver_io.h"
#include "./driver_systick/driver_systick.h"
#include "./driver_adc/driver_adc.h"
#include "stm32f0xx_hal.h"
#include "app_param.h"

#define R   (1 << 0)
#define W   (1 << 1)
#define MODBUS_SLAVE_ADDR_APPLY_DELAY_MS    50U

static void ModbusSetLed1(uint16_t value);
static void ModbusSetLed2(uint16_t value);
static void ModbusSetLed3(uint16_t value);
static void ModbusSetSlaveAddr(uint16_t value);
static void ModbusGetLed1(uint16_t *value);
static void ModbusGetLed2(uint16_t *value);
static void ModbusGetLed3(uint16_t *value);
static void ModbusGetSlaveAddr(uint16_t *value);
static eMBErrorCode ReadRegsCb(uint16_t startAddr, uint16_t regNum, uint8_t *buf);
static eMBErrorCode WriteRegsCb(uint16_t startAddr, uint16_t regNum, uint8_t *buf);
static void ModbusApplySlaveAddr(uint8_t slaveAddr);

static uint8_t g_modbusSlaveAddr = APP_PARAM_DEFAULT_SLAVE_ADDR;
static uint8_t g_pendingSlaveAddr = APP_PARAM_DEFAULT_SLAVE_ADDR;
static uint8_t g_isSlaveAddrPending = 0;
static uint32_t g_slaveAddrPendingTick = 0;

typedef struct {
    uint16_t property;
    const uint16_t address;
    uint16_t minValue;
    uint16_t maxValue;
    void (*ReadCb)(uint16_t *value);
    void (*WriteCb)(uint16_t value);
} MbRegisterInstance_t;

static void ModbusSetBeep1(uint16_t value);
static void ModbusSetBeep2(uint16_t value);
static void ModbusGetBeep1(uint16_t *value);
static void ModbusGetBeep2(uint16_t *value);
static void ModbusGetADC_CH1(uint16_t *value);
static void ModbusGetADC_CH2(uint16_t *value);
static MbRegisterInstance_t g_MbRegisterInstanceTable[] =
{
    {.property = R | W, .address = 0x0000, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed1,     .WriteCb = ModbusSetLed1,},
    {.property = R | W, .address = 0x0001, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed2,     .WriteCb = ModbusSetLed2,},
    {.property = R | W, .address = 0x0002, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed3,     .WriteCb = ModbusSetLed3,},
    {.property = R | W, .address = 0x0003, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetBeep1,    .WriteCb = ModbusSetBeep1,},
    {.property = R | W, .address = 0x0004, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetBeep2,    .WriteCb = ModbusSetBeep2,},
    {.property = R,     .address = 0x0005, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetADC_CH1,  .WriteCb = NULL,},
    {.property = R,     .address = 0x0006, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetADC_CH2,  .WriteCb = NULL,},
    {.property = R | W, .address = 0x0007, .minValue = APP_PARAM_MIN_SLAVE_ADDR, .maxValue = APP_PARAM_MAX_SLAVE_ADDR, .ReadCb = ModbusGetSlaveAddr, .WriteCb = ModbusSetSlaveAddr,},
};
#define REG_TABLE_SIZE (sizeof(g_MbRegisterInstanceTable) / sizeof(g_MbRegisterInstanceTable[0]))

static MbRegisterInstance_t *ModbusFindRegister(uint16_t address)
{
    for (uint32_t i = 0; i < REG_TABLE_SIZE; i++)
    {
        if (g_MbRegisterInstanceTable[i].address == address)
        {
            return &g_MbRegisterInstanceTable[i];
        }
    }

    return NULL;
}

static void ModbusSetLed1(uint16_t value) { GPIODriverWrite(IO_LED1, (uint8_t)value); }
static void ModbusSetLed2(uint16_t value) { GPIODriverWrite(IO_LED2, (uint8_t)value); }
static void ModbusSetLed3(uint16_t value) { GPIODriverWrite(IO_LED3, (uint8_t)value); }

static void ModbusSetSlaveAddr(uint16_t value)
{
    uint8_t slaveAddr = (uint8_t)value;

    if (AppParamSetSlaveAddr(slaveAddr) == ESUCCEES)
    {
        g_pendingSlaveAddr = slaveAddr;
        g_isSlaveAddrPending = 1;
        g_slaveAddrPendingTick = SysTickGetTick();
    }
}

static void ModbusGetLed1(uint16_t *value) { *value = (uint16_t)GPIODriverRead(IO_LED1); }
static void ModbusGetLed2(uint16_t *value) { *value = (uint16_t)GPIODriverRead(IO_LED2); }
static void ModbusGetLed3(uint16_t *value) { *value = (uint16_t)GPIODriverRead(IO_LED3); }
static void ModbusGetSlaveAddr(uint16_t *value) { *value = AppParamGetSlaveAddr(); }

static void ModbusApplySlaveAddr(uint8_t slaveAddr)
{
    ModbusSlaveInstance_t mbInstace = {
        .baudRate = 9600,
        .cb.ReadRegs = ReadRegsCb,
        .cb.WriteRegs = WriteRegsCb,
        .slaveAddr = slaveAddr,
    };

    eMBDisable();
    eMBClose();
    ModbusSlaveInit(&mbInstace);
    g_modbusSlaveAddr = slaveAddr;
}

static eMBErrorCode ReadRegsCb(uint16_t startAddr, uint16_t regNum, uint8_t *buf)
{
    if ((buf == NULL) || (regNum == 0))
    {
        return MB_EINVAL;
    }

    for (uint16_t i = 0; i < regNum; i++)
    {
        MbRegisterInstance_t *instance = ModbusFindRegister(startAddr + i);
        uint16_t val = 0;

        if (instance == NULL)
        {
            return MB_ENOREG;
        }
        if (((instance->property & R) == 0) || (instance->ReadCb == NULL))
        {
            return MB_EINVAL;
        }

        instance->ReadCb(&val);
        buf[2U * i] = (uint8_t)((val >> 8) & 0xFFU);
        buf[2U * i + 1U] = (uint8_t)(val & 0xFFU);
    }

    return MB_ENOERR;
}

static eMBErrorCode WriteRegsCb(uint16_t startAddr, uint16_t regNum, uint8_t *buf)
{
    if ((buf == NULL) || (regNum == 0))
    {
        return MB_EINVAL;
    }

    for (uint16_t i = 0; i < regNum; i++)
    {
        MbRegisterInstance_t *instance = ModbusFindRegister(startAddr + i);
        uint16_t setValue = ((uint16_t)buf[2U * i] << 8) | (uint16_t)buf[2U * i + 1U];

        if (instance == NULL)
        {
            return MB_ENOREG;
        }
        if (((instance->property & W) == 0) || (instance->WriteCb == NULL))
        {
            return MB_EINVAL;
        }
        if ((setValue < instance->minValue) || (setValue > instance->maxValue))
        {
            return MB_EINVAL;
        }

        instance->WriteCb(setValue);
    }

    return MB_ENOERR;
}

void ModbusAppInit(void)
{
    AppParamInit();
    ModbusApplySlaveAddr(AppParamGetSlaveAddr());
}

void ModbusTask(void)
{
    (void)eMBPoll();

    if ((g_isSlaveAddrPending != 0) &&
        ((SysTickGetTick() - g_slaveAddrPendingTick) >= MODBUS_SLAVE_ADDR_APPLY_DELAY_MS))
    {
        g_isSlaveAddrPending = 0;
        if (g_pendingSlaveAddr != g_modbusSlaveAddr)
        {
            ModbusApplySlaveAddr(g_pendingSlaveAddr);
        }
    }
}
static void ModbusSetBeep1(uint16_t value) { GPIODriverWrite(IO_BEEP1, (uint8_t)value); }
static void ModbusSetBeep2(uint16_t value) { GPIODriverWrite(IO_BEEP2, (uint8_t)value); }
static void ModbusGetBeep1(uint16_t *value) { *value = (uint16_t)GPIODriverRead(IO_BEEP1); }
static void ModbusGetBeep2(uint16_t *value) { *value = (uint16_t)GPIODriverRead(IO_BEEP2); }
static void ModbusGetADC_CH1(uint16_t *value) { *value = ADCDriverRead(ADC_CHANNEL_1); }
static void ModbusGetADC_CH2(uint16_t *value) { *value = ADCDriverRead(ADC_CHANNEL_2); }



