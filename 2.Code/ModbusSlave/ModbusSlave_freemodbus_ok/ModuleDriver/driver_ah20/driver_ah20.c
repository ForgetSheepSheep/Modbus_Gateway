#include "./driver_iic/driver_iic.h"
#include "./driver_ah20/driver_ah20.h"
#include "stm32f0xx_hal.h"

#define AHT20_ADDR          0x38
#define AHT20_WRITE_ADDR    (AHT20_ADDR << 1)
#define AHT20_READ_ADDR     ((AHT20_ADDR << 1) | 1)

void AHT20DriverInitial(void)
{
    IICDriverInit();
    HAL_Delay(40);  /* 上电后等待40ms才能读取状态 */

    /* 发送初始化命令 0xBE 0x08 0x00 */
    uint8_t initCmd[] = {0xBE, 0x08, 0x00};
    IICDriverWrite(AHT20_WRITE_ADDR, initCmd, sizeof(initCmd));

    /* 检查校准位 bit[3]，若未校准则重新初始化 */
    uint8_t status = 0;
    IICDriverRead(AHT20_READ_ADDR, &status, 1);
    if ((status & 0x08) != 0x08)
    {
        IICDriverWrite(AHT20_WRITE_ADDR, initCmd, sizeof(initCmd));
    }
}

void AHT20StartMeasure(void)
{
    uint8_t cmd[] = {0xAC, 0x33, 0x00};
    IICDriverWrite(AHT20_WRITE_ADDR, cmd, sizeof(cmd));
}

uint8_t AHT20IsReady(void)
{
    uint8_t status = 0;
    IICDriverRead(AHT20_READ_ADDR, &status, 1);
    return (status & 0x80) ? 0 : 1;
}

void AHT20TempHimGetData(float *temp, float *him)
{
    uint8_t buf[6];
    IICDriverRead(AHT20_READ_ADDR, buf, sizeof(buf));

    if ((buf[0] & 0x10) != 0x10)
        return;

    uint32_t rawHum = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    uint32_t rawTemp = (((uint32_t)(buf[3] & 0x0F)) << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    *him  = rawHum  * 100.0f / 1048576.0f;
    *temp = rawTemp * 200.0f / 1048576.0f - 50.0f;
}
