#include "app_modbusslave.h"
#include "modbus_slave.h"
#include "./driver_io/driver_io.h"
#include "app_task.h"
#include "./driver_adc/driver_adc.h"
#define R   (1 << 0)
#define W   (1 << 1)
static void ModbusSetLed1(uint16_t valve);
static void ModbusSetLed2(uint16_t valve);
static void ModbusSetLed3(uint16_t valve);
static void ModbusSetRelay1(uint16_t valve);
static void ModbusGetLed1(uint16_t *value);
static void ModbusGetLed2(uint16_t *value);
static void ModbusGetLed3(uint16_t *value);
static void ModbusGetRelay1(uint16_t *value);
static void ModbusGetAHT20Temp(uint16_t *value);
static void ModbusGetAHT20Humi(uint16_t *value);
static void ModbusGetADC_CH1(uint16_t *value);
static void ModbusGetADC_CH2(uint16_t *value);
typedef struct {
    uint16_t  property;
    const uint16_t address;
    uint16_t minValue;
    uint16_t maxValue;
	void (*ReadCb)(uint16_t *value);
	void (*WriteCb)(uint16_t value);
} MbRegisterInstance_t;


static MbRegisterInstance_t g_MbRegisterInstanceTable[] =
{
    {.property = R | W, .address = 0x0000, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed1, .WriteCb = ModbusSetLed1,},
    {.property = R | W, .address = 0x0001, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed2, .WriteCb = ModbusSetLed2,},
    {.property = R | W, .address = 0x0002, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetLed3, .WriteCb = ModbusSetLed3,},
    {.property = R | W, .address = 0x0003, .minValue = 0, .maxValue = 1, .ReadCb = ModbusGetRelay1, .WriteCb = ModbusSetRelay1,},
    {.property = R,     .address = 0x0004, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetAHT20Temp, .WriteCb = NULL,},
    {.property = R,     .address = 0x0005, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetAHT20Humi, .WriteCb = NULL,},
    {.property = R,     .address = 0x0006, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetADC_CH1,   .WriteCb = NULL,},
    {.property = R,     .address = 0x0007, .minValue = 0, .maxValue = 0, .ReadCb = ModbusGetADC_CH2,   .WriteCb = NULL,},
};

#define REG_TABLE_SIZE         (sizeof(g_MbRegisterInstanceTable) / sizeof(g_MbRegisterInstanceTable[0]))


static void ModbusSetLed1(uint16_t valve)
{
    GPIODriverWrite(IO_LED1, (uint8_t)valve);
}
static void ModbusSetLed2(uint16_t valve)
{
    GPIODriverWrite(IO_LED2, (uint8_t)valve);
}
static void ModbusSetLed3(uint16_t valve)
{
    GPIODriverWrite(IO_LED3, (uint8_t)valve);
}

static void ModbusSetRelay1(uint16_t valve)
{
    GPIODriverWrite(IO_RELAY1, (uint8_t)valve);
}

static void ModbusGetLed1(uint16_t *value)
{
    *value = (uint16_t)GPIODriverRead(IO_LED1);
}
static void ModbusGetLed2(uint16_t *value)
{
    *value = (uint16_t)GPIODriverRead(IO_LED2);
}
static void ModbusGetLed3(uint16_t *value)
{
    *value = (uint16_t)GPIODriverRead(IO_LED3);
}
static void ModbusGetRelay1(uint16_t *value)
{
    *value = (uint16_t)GPIODriverRead(IO_RELAY1);
}

static void ModbusGetAHT20Temp(uint16_t *value)
{
    *value = AppTaskGetAHT20Temp();
}

static void ModbusGetAHT20Humi(uint16_t *value)
{
    *value = AppTaskGetAHT20Humi();
}

static void ModbusGetADC_CH1(uint16_t *value)
{
    *value = ADCDriverRead(ADC_CHANNEL_1);
}

static void ModbusGetADC_CH2(uint16_t *value)
{
    *value = ADCDriverRead(ADC_CHANNEL_2);
}

static eMBErrorCode ReadRegsCb(uint8_t startAddr, uint8_t regNum, uint8_t *buf)
{
	if (buf == NULL)
	{
        return MB_EINVAL;
	}
	
    for (uint32_t i = 0; i < regNum; i++)
	{
		MbRegisterInstance_t *instance = NULL;
		for (uint32_t j = 0; j < REG_TABLE_SIZE; j++)
		{
			if (g_MbRegisterInstanceTable[j].address != startAddr + i)
			{
				continue;
			}
			instance = &g_MbRegisterInstanceTable[j];
			if((instance->property & R) == 0)  //��д����
			{
				return MB_EINVAL;
			}

			if (instance->ReadCb != NULL)
			{
				uint16_t val;
				instance->ReadCb(&val);
				buf[2 * i]     = (val >> 8) & 0xFF;
				buf[2 * i + 1] = val & 0xFF;
			}
		}
		if (instance == NULL)
		{
			return MB_ENOREG;
		}
		
    }

    return MB_ENOERR;

}
static eMBErrorCode WriteRegsCb(uint8_t startAddr, uint8_t regNum, uint8_t *buf)
{
    if (buf == NULL)
	{
        return MB_EINVAL;
	}
	
    for (uint32_t i = 0; i < regNum; i++)
	{
		
		MbRegisterInstance_t *instance = NULL;
		for (uint32_t j = 0; j < REG_TABLE_SIZE; j++)
		{
			if (g_MbRegisterInstanceTable[j].address != startAddr + i)
			{
				continue;
			}
			instance = &g_MbRegisterInstanceTable[j];
			
		    if((instance->property & W) == 0)  //��д����
			{
				return MB_EINVAL;
			}
			
			uint16_t setValue = ((buf[2 * i] << 8) & 0xFF00) | (buf[2 * i + 1] & 0xFF);
			if ((setValue < instance->minValue) || (setValue > instance->maxValue))
			{
				return MB_EINVAL;
			}
			
			if (instance->WriteCb != NULL)
			{
				instance->WriteCb(setValue);
			}
		}
		
		if (instance == NULL)
		{
			return MB_ENOREG;
		}
	}	
	return MB_ENOERR;
}

void ModbusAppInit(void)
{
	ModbusSlaveInstance_t mbInstace = {
        .baudRate = 9600,
        .cb.ReadRegs = ReadRegsCb,
        .cb.WriteRegs = WriteRegsCb,
        .slaveAddr = 0x01,
    };
	ModbusSlaveInit(&mbInstace);
}

void ModbusTask(void)
{
	(void)eMBPoll();
}

