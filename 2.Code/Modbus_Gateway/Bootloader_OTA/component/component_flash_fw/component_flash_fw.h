#ifndef __COMPONENT_FLASH_FW_H
#define __COMPONENT_FLASH_FW_H
#include "config.h"

/* 固件信息头存放地址 */
#define FLASH_FW_INFO_ADDR          0x0F0000UL
#define FLASH_FW_INFO_MAGIC         0x46574D47UL    /* 'FWMG' */

/* 外部Flash固件下载区规划 */
#define FLASH_FW_DOWNLOAD_ADDR      0x000000UL
#define FLASH_FW_AREA_SIZE          (512UL * 1024UL)

typedef struct
{
    uint32_t magic;
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint32_t fw_version;
    uint32_t fw_addr;
    uint32_t reserved[3];

} FlashFwInfo_t;

uint8_t  FlashFwInit(void);
uint8_t  FlashFwEraseDownloadArea(void);
uint8_t  FlashFwWriteDownload(uint32_t offset, uint8_t *buf, uint32_t len);
uint8_t  FlashFwReadDownload(uint32_t offset, uint8_t *buf, uint32_t len);
void     FlashFwDebugDumpRaw(uint32_t addr, uint32_t len);
uint32_t FlashFwCalcDownloadCRC32(uint32_t fw_size);
uint8_t  FlashFwWriteInfo(FlashFwInfo_t *info);
uint8_t  FlashFwReadInfo(FlashFwInfo_t *info);
uint8_t  FlashFwCheckInfo(FlashFwInfo_t *info);

#endif /* __COMPONENT_FLASH_FW_H */
