#include "stm32f0xx_hal.h"
#include "i2c.h"
#include "./driver_iic/driver_iic.h"

#define IIC_TIMEOUT_MS 100U

int IICDriverInit(void)
{
    return 0;
}

int IICDriverWrite(uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    if(pData == NULL || Size == 0 || DevAddress == 0)
        return -1;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, DevAddress, pData, Size, IIC_TIMEOUT_MS);
    if(status != HAL_OK) return -1;

    return 0;
}

int IICDriverRead(uint16_t DevAddress, uint8_t *pData, uint16_t Size)
{
    if(pData == NULL || Size == 0 || DevAddress == 0)
        return -1;

    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c1, DevAddress, pData, Size, IIC_TIMEOUT_MS);
    if(status != HAL_OK) return -1;

    return 0;
}
