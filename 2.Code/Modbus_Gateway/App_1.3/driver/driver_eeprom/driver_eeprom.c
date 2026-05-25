#include "./driver_eeprom/driver_eeprom.h"
#include "./driver_delay/driver_delay.h"

/* I2C引脚定义: PB6=SCL, PB7=SDA */
#define I2C_SCL_PIN     GPIO_PIN_6
#define I2C_SDA_PIN     GPIO_PIN_7
#define I2C_GPIO_PORT   GPIOB

#define GET_I2C_SDA()   gpio_input_bit_get(I2C_GPIO_PORT, I2C_SDA_PIN)
#define SET_I2C_SCL()   gpio_bit_set(I2C_GPIO_PORT, I2C_SCL_PIN)
#define CLR_I2C_SCL()   gpio_bit_reset(I2C_GPIO_PORT, I2C_SCL_PIN)
#define SET_I2C_SDA()   gpio_bit_set(I2C_GPIO_PORT, I2C_SDA_PIN)
#define CLR_I2C_SDA()   gpio_bit_reset(I2C_GPIO_PORT, I2C_SDA_PIN)

#define EEPROM_DEV_ADDR     0xA0
#define EEPROM_PAGE_SIZE    8
#define EEPROM_SIZE         256
#define EEPROM_I2C_WR       0
#define EEPROM_I2C_RD       1

static void I2C_GpioInit(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(I2C_GPIO_PORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_10MHZ, I2C_SCL_PIN | I2C_SDA_PIN);
}

static void I2C_Start(void)
{
    SET_I2C_SDA();
    SET_I2C_SCL();
    Delay_us(4);
    CLR_I2C_SDA();
    Delay_us(4);
    CLR_I2C_SCL();
    Delay_us(4);
}

static void I2C_Stop(void)
{
    CLR_I2C_SDA();
    Delay_us(4);
    SET_I2C_SCL();
    Delay_us(4);
    SET_I2C_SDA();
}

static void I2C_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (byte & 0x80)
        {
            SET_I2C_SDA();
        }
        else
        {
            CLR_I2C_SDA();
        }
        byte <<= 1;
        Delay_us(4);
        SET_I2C_SCL();
        Delay_us(4);
        CLR_I2C_SCL();
        Delay_us(4);
    }
}

static uint8_t I2C_ReadByte(void)
{
    uint8_t byte = 0;
    SET_I2C_SDA();
    for (uint8_t i = 0; i < 8; i++)
    {
        SET_I2C_SCL();
        Delay_us(4);
        byte <<= 1;
        if (GET_I2C_SDA())
        {
            byte++;
        }
        CLR_I2C_SCL();
        Delay_us(4);
    }
    return byte;
}

static uint8_t I2C_WaitAck(void)
{
    uint8_t errTimes = 0;

    SET_I2C_SDA();
    Delay_us(4);
    SET_I2C_SCL();
    Delay_us(4);

    while (GET_I2C_SDA())
    {
        errTimes++;
        if (errTimes > 250)
        {
            I2C_Stop();
            return EFAIL;
        }
    }

    CLR_I2C_SCL();
    Delay_us(4);
    return ESUCCESS;
}

static void I2C_SendAck(void)
{
    CLR_I2C_SDA();
    Delay_us(4);
    SET_I2C_SCL();
    Delay_us(4);
    CLR_I2C_SCL();
    Delay_us(4);
    SET_I2C_SDA();
}

static void I2C_SendNack(void)
{
    SET_I2C_SDA();
    Delay_us(4);
    SET_I2C_SCL();
    Delay_us(4);
    CLR_I2C_SCL();
}

void DrvEepromInit(void)
{
    I2C_GpioInit();
}

uint8_t DrvEepromRead(uint8_t readAddr, uint8_t *pBuffer, uint16_t numToRead)
{
    if ((readAddr + numToRead) > EEPROM_SIZE || pBuffer == NULL)
    {
        return EFAIL;
    }

    I2C_Start();
    I2C_SendByte(EEPROM_DEV_ADDR | EEPROM_I2C_WR);
    if (I2C_WaitAck() != ESUCCESS)
    {
        goto i2c_err;
    }

    I2C_SendByte(readAddr);
    if (I2C_WaitAck() != ESUCCESS)
    {
        goto i2c_err;
    }

    I2C_Start();
    I2C_SendByte(EEPROM_DEV_ADDR | EEPROM_I2C_RD);
    if (I2C_WaitAck() != ESUCCESS)
    {
        goto i2c_err;
    }

    numToRead--;
    while (numToRead--)
    {
        *pBuffer++ = I2C_ReadByte();
        I2C_SendAck();
    }
    *pBuffer = I2C_ReadByte();
    I2C_SendNack();

    I2C_Stop();
    return ESUCCESS;

i2c_err:
    I2C_Stop();
    return EFAIL;
}

uint8_t DrvEepromWrite(uint8_t writeAddr, uint8_t *pBuffer, uint16_t numToWrite)
{
    if ((writeAddr + numToWrite) > EEPROM_SIZE || pBuffer == NULL)
    {
        return EFAIL;
    }

    uint16_t i, j;
    uint8_t dataAddr = writeAddr;

    for (i = 0; i < numToWrite; i++)
    {
        if ((i == 0) || (dataAddr & (EEPROM_PAGE_SIZE - 1)) == 0)
        {
            I2C_Stop();

            for (j = 0; j < 100; j++)
            {
                I2C_Start();
                I2C_SendByte(EEPROM_DEV_ADDR | EEPROM_I2C_WR);
                if (I2C_WaitAck() == ESUCCESS)
                {
                    break;
                }
            }
            if (j == 100)
            {
                goto i2c_err;
            }

            I2C_SendByte(dataAddr);
            if (I2C_WaitAck() != ESUCCESS)
            {
                goto i2c_err;
            }
        }

        I2C_SendByte(pBuffer[i]);
        if (I2C_WaitAck() != ESUCCESS)
        {
            goto i2c_err;
        }

        dataAddr++;
    }

    I2C_Stop();
    return ESUCCESS;

i2c_err:
    I2C_Stop();
    return EFAIL;
}

