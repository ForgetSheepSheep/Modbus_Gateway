#include "./driver_iic/driver_iic.h"
#include "./driver_ah20/driver_ah20.h"
#include "./driver_systick/driver_systick.h"
#include "stm32f0xx_hal.h"

#define AHT20_ADDR              0x38
#define AHT20_WRITE_ADDR        (AHT20_ADDR << 1)
#define AHT20_READ_ADDR         ((AHT20_ADDR << 1) | 1)
#define AHT20_STATUS_BUSY       0x80
#define AHT20_STATUS_CALIBRATED 0x08
#define AHT20_MEASURE_PERIOD_MS 1000U
#define AHT20_MEASURE_DELAY_MS  80U
#define AHT20_MEASURE_TIMEOUT_MS 200U

typedef enum {
    AHT20_IDLE,
    AHT20_WAIT_READY,
} AHT20_State_t;

static AHT20_State_t g_aht20State = AHT20_IDLE;
static uint32_t g_aht20Tick = 0;
static uint16_t g_aht20Temp = 0;
static uint16_t g_aht20Humi = 0;

void AHT20DriverInitial(void)
{
    uint8_t initCmd[] = {0xBE, 0x08, 0x00};
    uint8_t status = 0;

    IICDriverInit();
    HAL_Delay(40);

    if (IICDriverWrite(AHT20_WRITE_ADDR, initCmd, sizeof(initCmd)) != 0)
    {
        return;
    }
    HAL_Delay(10);

    if ((IICDriverRead(AHT20_READ_ADDR, &status, 1) != 0) ||
        ((status & AHT20_STATUS_CALIBRATED) == 0))
    {
        (void)IICDriverWrite(AHT20_WRITE_ADDR, initCmd, sizeof(initCmd));
        HAL_Delay(10);
    }
}

void AHT20DriverTask(void)
{
    uint32_t now = SysTickGetTick();

    switch (g_aht20State)
    {
        case AHT20_IDLE:
            if ((now - g_aht20Tick) >= AHT20_MEASURE_PERIOD_MS)
            {
                AHT20StartMeasure();
                g_aht20Tick = now;
                g_aht20State = AHT20_WAIT_READY;
            }
            break;

        case AHT20_WAIT_READY:
            if ((now - g_aht20Tick) >= AHT20_MEASURE_DELAY_MS)
            {
                if (AHT20IsReady() != 0)
                {
                    float temp = 0.0f;
                    float humi = 0.0f;

                    if (AHT20ReadTempHumi(&temp, &humi) != 0)
                    {
                        g_aht20Temp = (uint16_t)(int16_t)(temp * 10.0f);
                        g_aht20Humi = (uint16_t)(humi * 10.0f);
                    }
                    g_aht20State = AHT20_IDLE;
                }
                else if ((now - g_aht20Tick) >= AHT20_MEASURE_TIMEOUT_MS)
                {
                    g_aht20State = AHT20_IDLE;
                }
            }
            break;

        default:
            g_aht20State = AHT20_IDLE;
            break;
    }
}

void AHT20StartMeasure(void)
{
    uint8_t cmd[] = {0xAC, 0x33, 0x00};
    (void)IICDriverWrite(AHT20_WRITE_ADDR, cmd, sizeof(cmd));
}

uint8_t AHT20IsReady(void)
{
    uint8_t status = 0;

    if (IICDriverRead(AHT20_READ_ADDR, &status, 1) != 0)
    {
        return 0;
    }

    return ((status & AHT20_STATUS_BUSY) == 0) ? 1U : 0U;
}

uint8_t AHT20ReadTempHumi(float *temp, float *him)
{
    uint8_t buf[6];
    uint32_t rawHum;
    uint32_t rawTemp;

    if ((temp == NULL) || (him == NULL))
    {
        return 0;
    }

    if (IICDriverRead(AHT20_READ_ADDR, buf, sizeof(buf)) != 0)
    {
        return 0;
    }

    if ((buf[0] & AHT20_STATUS_BUSY) != 0)
    {
        return 0;
    }

    rawHum = ((uint32_t)buf[1] << 12) |
             ((uint32_t)buf[2] << 4) |
             ((uint32_t)buf[3] >> 4);
    rawTemp = (((uint32_t)buf[3] & 0x0FU) << 16) |
              ((uint32_t)buf[4] << 8) |
              (uint32_t)buf[5];

    *him = rawHum * 100.0f / 1048576.0f;
    *temp = rawTemp * 200.0f / 1048576.0f - 50.0f;

    return 1;
}

void AHT20TempHimGetData(float *temp, float *him)
{
    (void)AHT20ReadTempHumi(temp, him);
}

uint16_t AHT20GetTemp(void)
{
    return g_aht20Temp;
}

uint16_t AHT20GetHumi(void)
{
    return g_aht20Humi;
}
