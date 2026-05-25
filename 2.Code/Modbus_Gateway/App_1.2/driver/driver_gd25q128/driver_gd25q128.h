#ifndef __DRIVER_GD25Q128_H
#define __DRIVER_GD25Q128_H

#include <stdint.h>



/* GD25Q128 容量参数 */
#define GD25Q128_SIZE              (16UL * 1024UL * 1024UL)   /* 16MB */
#define GD25Q128_SECTOR_SIZE       (4UL * 1024UL)             /* 4KB */
#define GD25Q128_PAGE_SIZE         256UL                      /* 256Byte */

/* Bootloader 固件下载区规划 */
#define GD25Q128_FW_DOWNLOAD_ADDR  0x000000UL                 /* 新固件下载区 */
#define GD25Q128_FW_BACKUP_ADDR    0x100000UL                 /* 旧APP备份区 */
#define GD25Q128_FW_AREA_SIZE      (512UL * 1024UL)           /* 512KB */

/* GD25Q128 常用命令 */
#define GD25Q128_CMD_WRITE_ENABLE      0x06
#define GD25Q128_CMD_WRITE_DISABLE     0x04
#define GD25Q128_CMD_READ_STATUS       0x05
#define GD25Q128_CMD_READ_ID           0x9F
#define GD25Q128_CMD_READ_DATA         0x03
#define GD25Q128_CMD_PAGE_PROGRAM      0x02
#define GD25Q128_CMD_SECTOR_ERASE      0x20
#define GD25Q128_CMD_BLOCK_ERASE_64K   0xD8
#define GD25Q128_CMD_CHIP_ERASE        0xC7

#define GD25Q128_CMD_READ_STATUS1       0x05    /* 读状态寄存器1 */
#define GD25Q128_STATUS_BUSY            0x01    /* WIP位，1表示忙 */
#define GD25Q128_STATUS_WEL             0x02    /* WEL位，1表示已经写使能 */

/* GD25Q128 JEDEC ID */
#define GD25Q128_JEDEC_ID          0xC84018UL

void DrvGD25Q128Init(void);

uint32_t DrvGD25Q128ReadID(void);


void DrvGD25Q128WriteEnable(void);
void DrvGD25Q128WriteDisable(void);
uint8_t DrvGD25Q128ReadStatus(void);
void DrvGD25Q128WaitBusy(void);

uint8_t DrvGD25Q128SectorErase(uint32_t addr);
uint8_t DrvGD25Q128BlockErase64K(uint32_t addr);

uint8_t DrvGD25Q128PageWrite(uint32_t addr, uint8_t *buf, uint16_t len);
uint8_t DrvGD25Q128WriteBuf(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t DrvGD25Q128ReadBuf(uint32_t addr, uint8_t *buf, uint32_t len);

uint8_t DrvGD25Q128EraseArea(uint32_t addr, uint32_t len);


#endif /* __DRIVER_GD25Q128_H */


