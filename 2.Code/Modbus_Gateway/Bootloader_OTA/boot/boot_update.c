#include "boot_update.h"
#include "boot_jump.h"
#include "component_flash_fw/component_flash_fw.h"
#include "./driver_flash/driver_flash.h"
#include "config.h"

#include <stdio.h>

/************************************************************
* @brief Copy valid firmware from external Flash to internal APP area.
* @return ESUCCESS if update succeeds, otherwise EFAIL.
************************************************************/
uint8_t BootUpdateFromExternalFlash(void)
{
    FlashFwInfo_t info;
    uint8_t buf[256];
    uint32_t offset = 0;
    uint32_t read_len;
    uint32_t crc32;

    printf("[UPDATE] check external flash firmware...\r\n");

    if(FlashFwInit() != ESUCCESS)
    {
        printf("[UPDATE] external flash init error\r\n");
        return EFAIL;
    }

    if(FlashFwReadInfo(&info) != ESUCCESS)
    {
        printf("[UPDATE] read firmware info error\r\n");
        return EFAIL;
    }

    printf("[UPDATE] info magic=0x%08X size=%u crc=0x%08X ver=%u addr=0x%08X\r\n",
           (unsigned int)info.magic,
           (unsigned int)info.fw_size,
           (unsigned int)info.fw_crc32,
           (unsigned int)info.fw_version,
           (unsigned int)info.fw_addr);
    FlashFwDebugDumpRaw(FLASH_FW_INFO_ADDR, sizeof(FlashFwInfo_t));

    if(FlashFwCheckInfo(&info) != ESUCCESS)
    {
        printf("[UPDATE] firmware info invalid\r\n");
        return EFAIL;
    }

    crc32 = FlashFwCalcDownloadCRC32(info.fw_size);
    if(crc32 != info.fw_crc32)
    {
        printf("[UPDATE] firmware crc error calc=0x%08X expect=0x%08X\r\n",
               (unsigned int)crc32,
               (unsigned int)info.fw_crc32);
        return EFAIL;
    }

    printf("[FLASH] erase app area...\r\n");

    if(DrvFlashErase(APP_START_ADDR, info.fw_size) != 0)
    {
        printf("[FLASH] erase app error\r\n");
        return EFAIL;
    }

    printf("[FLASH] copy firmware...\r\n");

    while(offset < info.fw_size)
    {
        if((info.fw_size - offset) > sizeof(buf))
        {
            read_len = sizeof(buf);
        }
        else
        {
            read_len = info.fw_size - offset;
        }

        if(FlashFwReadDownload(offset, buf, read_len) != ESUCCESS)
        {
            printf("[FLASH] read ext flash error\r\n");
            return EFAIL;
        }

        if(DrvFlashWrite(APP_START_ADDR + offset, buf, read_len) != 0)
        {
            printf("[FLASH] write app error\r\n");
            return EFAIL;
        }

        offset += read_len;
    }

    if(BootCheckAppValid(APP_START_ADDR) != 1)
    {
        printf("NEW APP INVALID\r\n");
        return EFAIL;
    }

    printf("NEW APP VALID\r\n");

    return ESUCCESS;
}
