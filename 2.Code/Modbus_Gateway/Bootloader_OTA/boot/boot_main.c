#include "boot_main.h"
#include "boot_jump.h"
#include "boot_update.h"
#include "driver_eeprom/driver_eeprom.h"

#include <stdio.h>
#include <string.h>

#define OTA_FLAG_ADDR       0x00
#define OTA_FLAG_MAGIC_0    0x5A
#define OTA_FLAG_MAGIC_1    0x5A
#define OTA_FLAG_MAGIC_2    0x5A
#define OTA_FLAG_MAGIC_3    0x5A

#define OTA_TITLE_ADDR      0x04
#define OTA_VERSION_ADDR    0x24
#define OTA_CHECKSUM_ADDR   0x44
#define OTA_SIZE_ADDR       0x64

typedef enum
{
    BOOT_STATE_INIT = 0,
    BOOT_STATE_CHECK_APP,
    BOOT_STATE_CHECK_OTA_FLAG,
    BOOT_STATE_UPDATE_FROM_EXT_FLASH,
    BOOT_STATE_JUMP_APP,
    BOOT_STATE_ERROR,

} BootState_e;

static BootState_e g_boot_state = BOOT_STATE_INIT;

void BootMainInit(void)
{
    g_boot_state = BOOT_STATE_INIT;
}

/************************************************************
* @brief Check AT24C02 for OTA upgrade flag
* @return 1 if OTA flag is set, 0 otherwise
************************************************************/
static uint8_t BootCheckOtaFlag(void)
{
    uint8_t flag[4];

    if(DrvEepromRead(OTA_FLAG_ADDR, flag, 4) != ESUCCESS)
    {
        return 0;
    }

    if(flag[0] == OTA_FLAG_MAGIC_0 &&
       flag[1] == OTA_FLAG_MAGIC_1 &&
       flag[2] == OTA_FLAG_MAGIC_2 &&
       flag[3] == OTA_FLAG_MAGIC_3)
    {
        return 1;
    }

    return 0;
}

/************************************************************
* @brief Clear AT24C02 OTA upgrade flag
************************************************************/
static void BootClearOtaFlag(void)
{
    uint8_t clear[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    DrvEepromWrite(OTA_FLAG_ADDR, clear, 4);
}

/************************************************************
* @brief Read and print OTA firmware info from AT24C02
************************************************************/
static void BootPrintOtaInfo(void)
{
    char buf[32];
    uint32_t fw_size = 0;

    DrvEepromRead(OTA_TITLE_ADDR, (uint8_t *)buf, 32);
    buf[31] = '\0';
    printf("[OTA] title: %s\r\n", buf);

    DrvEepromRead(OTA_VERSION_ADDR, (uint8_t *)buf, 32);
    buf[31] = '\0';
    printf("[OTA] version: %s\r\n", buf);

    DrvEepromRead(OTA_CHECKSUM_ADDR, (uint8_t *)buf, 32);
    buf[31] = '\0';
    printf("[OTA] checksum: %s\r\n", buf);

    DrvEepromRead(OTA_SIZE_ADDR, (uint8_t *)&fw_size, 4);
    printf("[OTA] size: %u\r\n", (unsigned int)fw_size);
}

void BootMainProcess(void)
{
    switch(g_boot_state)
    {
        case BOOT_STATE_INIT:
        {
            printf("BOOT STATE INIT\r\n");
            DrvEepromInit();
            g_boot_state = BOOT_STATE_CHECK_APP;
            break;
        }

        case BOOT_STATE_CHECK_APP:
        {
            if(BootCheckAppValid(APP_START_ADDR) == 1)
            {
                printf("APP VALID\r\n");
                g_boot_state = BOOT_STATE_CHECK_OTA_FLAG;
            }
            else
            {
                printf("APP INVALID\r\n");
                g_boot_state = BOOT_STATE_UPDATE_FROM_EXT_FLASH;
            }
            break;
        }

        case BOOT_STATE_CHECK_OTA_FLAG:
        {
            if(BootCheckOtaFlag() == 1)
            {
                printf("OTA FLAG DETECTED\r\n");
                BootPrintOtaInfo();
                BootClearOtaFlag();
                printf("OTA FLAG CLEARED\r\n");
                g_boot_state = BOOT_STATE_UPDATE_FROM_EXT_FLASH;
            }
            else
            {
                g_boot_state = BOOT_STATE_JUMP_APP;
            }
            break;
        }

        case BOOT_STATE_UPDATE_FROM_EXT_FLASH:
        {
            if(BootUpdateFromExternalFlash() == ESUCCESS)
            {
                printf("JUMP TO NEW APP\r\n");
                g_boot_state = BOOT_STATE_JUMP_APP;
            }
            else
            {
                printf("UPDATE FROM EXT FLASH FAIL\r\n");
                g_boot_state = BOOT_STATE_ERROR;
            }
            break;
        }

        case BOOT_STATE_JUMP_APP:
        {
            printf("JUMP TO APP\r\n");
            BootJumpToApp(APP_START_ADDR);
            break;
        }

        case BOOT_STATE_ERROR:
        {
            printf("BOOT STATE ERROR\r\n");
            break;
        }

        default:
        {
            g_boot_state = BOOT_STATE_ERROR;
            break;
        }
    }
}
