#include "component_ota/component_ota.h"
#include "component_flash_fw/component_flash_fw.h"
#include "component_crc32/component_crc32.h"
#include "driver_eeprom/driver_eeprom.h"
#include "driver_delay/driver_delay.h"
#include "app_4g.h"
#include "bsp_tick/bsp_tick.h"
#include "cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTA_CHUNK_SIZE       256U
#define OTA_RECV_TIMEOUT_MS  10000U
#define OTA_PUB_TOPIC        "v2/fw/request/+/chunk/#"
#define OTA_SUB_TOPIC_PREFIX "v2/fw/response/"

OtaInfo_t g_ota_info;

void ComponentOtaInit(void)
{
    memset(&g_ota_info, 0, sizeof(g_ota_info));
}

/************************************************************
* @brief 解析MQTT attributes下发的OTA通知JSON
* @param json JSON字符串，包含 fw_title/fw_version/fw_size/fw_checksum
* @return ESUCCESS解析成功，EFAIL失败
************************************************************/
uint8_t ComponentOtaParseNotify(uint8_t *json)
{
    cJSON *root = NULL;
    cJSON *title = NULL;
    cJSON *version = NULL;
    cJSON *size = NULL;
    cJSON *checksum = NULL;

    if(json == NULL)
    {
        return EFAIL;
    }

    root = cJSON_Parse((char *)json);
    if(root == NULL)
    {
        return EFAIL;
    }

    title = cJSON_GetObjectItem(root, "fw_title");
    if(title == NULL || title->valuestring == NULL)
    {
        cJSON_Delete(root);
        return EFAIL;
    }

    version = cJSON_GetObjectItem(root, "fw_version");
    size = cJSON_GetObjectItem(root, "fw_size");
    checksum = cJSON_GetObjectItem(root, "fw_checksum");

    strncpy(g_ota_info.fw_title, title->valuestring, sizeof(g_ota_info.fw_title) - 1);

    if(version != NULL && version->valuestring != NULL)
    {
        strncpy(g_ota_info.fw_version, version->valuestring, sizeof(g_ota_info.fw_version) - 1);
    }

    if(size != NULL)
    {
        g_ota_info.fw_size = (uint32_t)size->valueint;
    }

    if(checksum != NULL && checksum->valuestring != NULL)
    {
        strncpy(g_ota_info.fw_checksum, checksum->valuestring, sizeof(g_ota_info.fw_checksum) - 1);
    }

    cJSON_Delete(root);

    printf("[OTA] title: %s\r\n", g_ota_info.fw_title);
    printf("[OTA] version: %s\r\n", g_ota_info.fw_version);
    printf("[OTA] size: %u\r\n", (unsigned int)g_ota_info.fw_size);
    printf("[OTA] checksum: %s\r\n", g_ota_info.fw_checksum);

    return ESUCCESS;
}

void ComponentOtaTrigger(void)
{
    g_ota_info.ota_trigger = 1;
}

void ComponentOtaSetRecvFlag(void)
{
    g_ota_info.recv_flag = 1;
}

void ComponentOtaFeedChunk(uint8_t *data, uint32_t len)
{
    if(data == NULL || len == 0 || len > OTA_CHUNK_SIZE)
    {
        return;
    }

    memcpy(g_ota_info.recv_buf, data, len);
    g_ota_info.recv_flag = 1;
}

/************************************************************
* @brief 发送一个固件分包请求
* @return ESUCCESS成功，EFAIL失败
************************************************************/
static uint8_t OtaSendChunkRequest(void)
{
    char topic[128];
    char payload[16];
    int len;

    len = snprintf(topic, sizeof(topic),
                   "v2/fw/request/%u/chunk/%u",
                   (unsigned int)g_ota_info.request_id,
                   (unsigned int)g_ota_info.chunk_id);

    if(len < 0 || len >= (int)sizeof(topic))
    {
        return EFAIL;
    }

    len = snprintf(payload, sizeof(payload), "%u",
                   (unsigned int)g_ota_info.request_bytes);

    if(len < 0 || len >= (int)sizeof(payload))
    {
        return EFAIL;
    }

    return App4GPushData(topic, payload);
}

/************************************************************
* @brief 设置AT24C02 OTA升级标志
* @return ESUCCESS成功，EFAIL失败
************************************************************/
static uint8_t OtaSetFlag(void)
{
    uint8_t flag[4] = OTA_FLAG_MAGIC;
    return DrvEepromWrite(OTA_FLAG_ADDR, flag, 4);
}

/************************************************************
* @brief OTA下载主流程（阻塞式）
* @return ESUCCESS升级成功（将重启），EFAIL失败
************************************************************/
static uint8_t OtaDownloadFirmware(void)
{
    uint32_t total_chunks;
    uint32_t crc32_calc;
    uint32_t crc32_expect;
    uint32_t write_offset;
    uint32_t valid_len;
    uint32_t timeout;
    FlashFwInfo_t fw_info;

    if(g_ota_info.fw_size == 0)
    {
        printf("[OTA] fw_size is 0\r\n");
        return EFAIL;
    }

    if(g_ota_info.fw_size > FLASH_FW_AREA_SIZE)
    {
        printf("[OTA] fw_size too large\r\n");
        return EFAIL;
    }

    /* 计算总分包数 */
    total_chunks = g_ota_info.fw_size / OTA_CHUNK_SIZE;
    if(g_ota_info.fw_size % OTA_CHUNK_SIZE != 0)
    {
        total_chunks++;
    }

    printf("[OTA] total chunks: %u\r\n", (unsigned int)total_chunks);

    /* 初始化Flash固件模块 */
    if(FlashFwInit() != ESUCCESS)
    {
        printf("[OTA] flash init error\r\n");
        return EFAIL;
    }

    /* 擦除下载区 */
    printf("[OTA] erase download area...\r\n");
    if(FlashFwEraseDownloadArea() != ESUCCESS)
    {
        printf("[OTA] erase error\r\n");
        return EFAIL;
    }

    /* 初始化CRC32 */
    crc32_calc = CRC32_INIT_VALUE;

    /* 生成随机request_id */
    g_ota_info.request_id = BspGetTick() & 0xFFFF;
    g_ota_info.chunk_id = 0;
    g_ota_info.received_bytes = 0;
    g_ota_info.request_bytes = OTA_CHUNK_SIZE;

    write_offset = 0;

    /* 逐包下载 */
    for(uint32_t i = 0; i < total_chunks; i++)
    {
        /* 最后一包可能不满256字节 */
        if(i == total_chunks - 1 && g_ota_info.fw_size % OTA_CHUNK_SIZE != 0)
        {
            g_ota_info.request_bytes = g_ota_info.fw_size % OTA_CHUNK_SIZE;
        }
        else
        {
            g_ota_info.request_bytes = OTA_CHUNK_SIZE;
        }

        /* 发送分包请求 */
        g_ota_info.recv_flag = 0;
        if(OtaSendChunkRequest() != ESUCCESS)
        {
            printf("[OTA] send chunk %u request error\r\n", (unsigned int)i);
            return EFAIL;
        }

        /* 等待分包响应 */
        timeout = OTA_RECV_TIMEOUT_MS;
        while(g_ota_info.recv_flag == 0)
        {
            Delay_ms(1);
            timeout--;
            if(timeout == 0)
            {
                printf("[OTA] chunk %u timeout\r\n", (unsigned int)i);
                return EFAIL;
            }
        }

        /* 计算有效长度 */
        valid_len = g_ota_info.request_bytes;

        /* 写入GD25Q128 */
        if(FlashFwWriteDownload(write_offset, g_ota_info.recv_buf, valid_len) != ESUCCESS)
        {
            printf("[OTA] write chunk %u error\r\n", (unsigned int)i);
            return EFAIL;
        }

        /* 增量CRC32 */
        crc32_calc = ComponentCRC32Update(crc32_calc, g_ota_info.recv_buf, valid_len);

        write_offset += valid_len;
        g_ota_info.received_bytes += valid_len;
        g_ota_info.chunk_id++;

        printf("[OTA] chunk %u/%u ok\r\n", (unsigned int)(i + 1), (unsigned int)total_chunks);
    }

    /* CRC32最终化 */
    crc32_calc ^= CRC32_XOR_VALUE;

    /* 大端字节序翻转后与服务器比对 */
    crc32_expect = (uint32_t)strtoul(g_ota_info.fw_checksum, NULL, 16);
    uint32_t crc32_swap = ((crc32_calc >> 24) & 0x000000FFU) |
                          ((crc32_calc >> 8)  & 0x0000FF00U) |
                          ((crc32_calc << 8)  & 0x00FF0000U) |
                          ((crc32_calc << 24) & 0xFF000000U);

    printf("[OTA] crc32 calc: 0x%08X\r\n", (unsigned int)crc32_calc);
    printf("[OTA] crc32 swap: 0x%08X\r\n", (unsigned int)crc32_swap);
    printf("[OTA] crc32 expect: 0x%08X\r\n", (unsigned int)crc32_expect);

    if(crc32_swap != crc32_expect)
    {
        printf("[OTA] crc32 mismatch!\r\n");
        return EFAIL;
    }

    /* 写固件信息头到GD25Q128 */
    memset(&fw_info, 0, sizeof(fw_info));
    fw_info.magic = FLASH_FW_INFO_MAGIC;
    fw_info.fw_size = g_ota_info.fw_size;
    fw_info.fw_crc32 = crc32_calc;
    fw_info.fw_version = 0;
    fw_info.fw_addr = FLASH_FW_DOWNLOAD_ADDR;

    if(FlashFwWriteInfo(&fw_info) != ESUCCESS)
    {
        printf("[OTA] write fw info error\r\n");
        return EFAIL;
    }

    printf("[OTA] fw info written\r\n");

    /* 写入完整固件信息到AT24C02 */
    if(OtaSetFlag() != ESUCCESS)
    {
        printf("[OTA] set eeprom flag error\r\n");
        return EFAIL;
    }

    uint8_t buf[32];

    memset(buf, 0, sizeof(buf));
    strncpy((char *)buf, g_ota_info.fw_title, sizeof(buf) - 1);
    DrvEepromWrite(OTA_TITLE_ADDR, buf, sizeof(buf));

    memset(buf, 0, sizeof(buf));
    strncpy((char *)buf, g_ota_info.fw_version, sizeof(buf) - 1);
    DrvEepromWrite(OTA_VERSION_ADDR, buf, sizeof(buf));

    memset(buf, 0, sizeof(buf));
    strncpy((char *)buf, g_ota_info.fw_checksum, sizeof(buf) - 1);
    DrvEepromWrite(OTA_CHECKSUM_ADDR, buf, sizeof(buf));

    uint32_t fw_size = g_ota_info.fw_size;
    DrvEepromWrite(OTA_SIZE_ADDR, (uint8_t *)&fw_size, 4);

    printf("[OTA] eeprom: title=%s ver=%s size=%u checksum=%s\r\n",
           g_ota_info.fw_title, g_ota_info.fw_version,
           (unsigned int)g_ota_info.fw_size, g_ota_info.fw_checksum);
    printf("[OTA] rebooting...\r\n");

    /* 上报升级成功 */
    App4GPushData(APP_4G_PUB_TELEMETRY, "{\"fw_state\":\"UPDATED\"}");

    Delay_ms(500);

    /* 重启 */
    NVIC_SystemReset();

    return ESUCCESS;
}

/************************************************************
* @brief OTA处理主函数，在主循环中调用
* @return ESUCCESS空闲，EFAIL正在处理或失败
************************************************************/
uint8_t ComponentOtaProcess(void)
{
    if(g_ota_info.ota_trigger == 0)
    {
        return ESUCCESS;
    }

    g_ota_info.ota_trigger = 0;

    printf("[OTA] start download\r\n");

    if(OtaDownloadFirmware() != ESUCCESS)
    {
        printf("[OTA] download failed\r\n");
        App4GPushData(APP_4G_PUB_TELEMETRY, "{\"fw_state\":\"FAILED\"}");
        return EFAIL;
    }

    return ESUCCESS;
}
