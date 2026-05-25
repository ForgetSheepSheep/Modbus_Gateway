#ifndef __COMPONENT_OTA_H
#define __COMPONENT_OTA_H

#include "config.h"

/* AT24C02 地址规划 */
#define OTA_FLAG_ADDR           0x00    /* 4字节:  OTA升级标志 */
#define OTA_TITLE_ADDR          0x04    /* 32字节: fw_title */
#define OTA_VERSION_ADDR        0x24    /* 32字节: fw_version */
#define OTA_CHECKSUM_ADDR       0x44    /* 32字节: fw_checksum */
#define OTA_SIZE_ADDR           0x64    /* 4字节:  fw_size (uint32_t) */
#define OTA_VERSION_MAX_LEN     32

#define OTA_FLAG_MAGIC          {0x5A, 0x5A, 0x5A, 0x5A}
#define OTA_FLAG_CLEAR          {0xFF, 0xFF, 0xFF, 0xFF}

typedef struct
{
    char     fw_title[32];
    char     fw_version[32];
    char     fw_checksum[32];
    uint32_t fw_size;
    uint32_t received_bytes;
    uint32_t request_id;
    uint32_t chunk_id;
    uint32_t request_bytes;
    uint32_t recv_len;
    uint8_t  recv_buf[256];
    volatile uint8_t recv_flag;
    uint8_t  ota_trigger;

} OtaInfo_t;

extern OtaInfo_t g_ota_info;

void    ComponentOtaInit(void);
uint8_t ComponentOtaParseNotify(uint8_t *json);
void    ComponentOtaTrigger(void);
uint8_t ComponentOtaProcess(void);
void    ComponentOtaSetRecvFlag(void);
void    ComponentOtaFeedChunk(uint8_t *data, uint32_t len);

#endif /* __COMPONENT_OTA_H */
