#include "app_param.h"
#include "stm32f0xx_hal.h"

#define APP_PARAM_MAGIC             0x4D424144UL
#define APP_PARAM_VERSION           0x0001U
#define APP_PARAM_FLASH_PAGE_ADDR   (FLASH_BANK1_END + 1U - FLASH_PAGE_SIZE)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t slaveAddr;
    uint32_t checksum;
} AppParamFlash_t;

static uint8_t g_slaveAddr = APP_PARAM_DEFAULT_SLAVE_ADDR;

static uint32_t AppParamCalcChecksum(const AppParamFlash_t *param)
{
    return param->magic ^ param->version ^ param->slaveAddr ^ 0xA5A55A5AUL;
}

uint8_t AppParamIsValidSlaveAddr(uint8_t slaveAddr)
{
    return ((slaveAddr >= APP_PARAM_MIN_SLAVE_ADDR) &&
            (slaveAddr <= APP_PARAM_MAX_SLAVE_ADDR));
}

void AppParamInit(void)
{
    const AppParamFlash_t *param = (const AppParamFlash_t *)APP_PARAM_FLASH_PAGE_ADDR;

    if ((param->magic == APP_PARAM_MAGIC) &&
        (param->version == APP_PARAM_VERSION) &&
        (param->checksum == AppParamCalcChecksum(param)) &&
        AppParamIsValidSlaveAddr((uint8_t)param->slaveAddr))
    {
        g_slaveAddr = (uint8_t)param->slaveAddr;
    }
    else
    {
        g_slaveAddr = APP_PARAM_DEFAULT_SLAVE_ADDR;
    }
}

uint8_t AppParamGetSlaveAddr(void)
{
    return g_slaveAddr;
}

int AppParamSetSlaveAddr(uint8_t slaveAddr)
{
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError = 0;
    AppParamFlash_t param;
    HAL_StatusTypeDef status;

    if (!AppParamIsValidSlaveAddr(slaveAddr))
    {
        return EFAIL;
    }

    if (slaveAddr == g_slaveAddr)
    {
        return ESUCCEES;
    }

    param.magic = APP_PARAM_MAGIC;
    param.version = APP_PARAM_VERSION;
    param.slaveAddr = slaveAddr;
    param.checksum = AppParamCalcChecksum(&param);

    HAL_FLASH_Unlock();

    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = APP_PARAM_FLASH_PAGE_ADDR;
    eraseInit.NbPages = 1;

    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status == HAL_OK)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   APP_PARAM_FLASH_PAGE_ADDR,
                                   param.magic);
    }
    if (status == HAL_OK)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   APP_PARAM_FLASH_PAGE_ADDR + 4U,
                                   ((uint32_t)param.slaveAddr << 16) | param.version);
    }
    if (status == HAL_OK)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   APP_PARAM_FLASH_PAGE_ADDR + 8U,
                                   param.checksum);
    }

    HAL_FLASH_Lock();

    if (status != HAL_OK)
    {
        return EFAIL;
    }

    g_slaveAddr = slaveAddr;
    return ESUCCEES;
}
