#include "component_ota/component_ota.h"
#include "component_flash_fw/component_flash_fw.h"
#include "component_crc32/component_crc32.h"
#include "driver_eeprom/driver_eeprom.h"
#include "driver_delay/driver_delay.h"
#include "driver_4g_uart/driver_4g_uart.h"
#include "app_4g.h"
#include "bsp_tick/bsp_tick.h"
#include "cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTA_CHUNK_SIZE       256U
#define OTA_RECV_TIMEOUT_MS  10000U

OtaInfo_t g_ota_info;

static void OtaPrintHex(const char *tag, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if(tag == NULL || buf == NULL)
    {
        return;
    }

    printf("%s", tag);
    for(i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\r\n");
}

static uint8_t OtaParseChecksumHex(const char *text, uint32_t *checksum)
{
    char *endp;
    unsigned long value;

    if(text == NULL || checksum == NULL || text[0] == '\0')
    {
        return EFAIL;
    }

    value = strtoul(text, &endp, 16);
    if(endp == text || *endp != '\0' || value > 0xFFFFFFFFUL)
    {
        return EFAIL;
    }

    *checksum = (uint32_t)value;
    return ESUCCESS;
}

static uint32_t OtaSwapU32(uint32_t value)
{
    return ((value >> 24) & 0x000000FFU) |
           ((value >> 8)  & 0x0000FF00U) |
           ((value << 8)  & 0x00FF0000U) |
           ((value << 24) & 0xFF000000U);
}

void ComponentOtaInit(void)
{
    memset(&g_ota_info, 0, sizeof(g_ota_info));
}

uint8_t ComponentOtaParseNotify(uint8_t *json)
{
    cJSON *root = NULL;
    cJSON *title = NULL;
    cJSON *version = NULL;
    cJSON *size = NULL;
    cJSON *checksum = NULL;
    char fw_title[sizeof(g_ota_info.fw_title)];
    char fw_version[sizeof(g_ota_info.fw_version)];
    char fw_checksum[sizeof(g_ota_info.fw_checksum)];
    uint32_t fw_size;

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
    version = cJSON_GetObjectItem(root, "fw_version");
    size = cJSON_GetObjectItem(root, "fw_size");
    checksum = cJSON_GetObjectItem(root, "fw_checksum");

    if(title == NULL || title->valuestring == NULL ||
       version == NULL || version->valuestring == NULL ||
       size == NULL || cJSON_IsNumber(size) == 0 ||
       checksum == NULL || checksum->valuestring == NULL)
    {
        cJSON_Delete(root);
        return EFAIL;
    }

    if(size->valueint <= 0)
    {
        cJSON_Delete(root);
        return EFAIL;
    }

    memset(fw_title, 0, sizeof(fw_title));
    memset(fw_version, 0, sizeof(fw_version));
    memset(fw_checksum, 0, sizeof(fw_checksum));

    strncpy(fw_title, title->valuestring, sizeof(fw_title) - 1);
    strncpy(fw_version, version->valuestring, sizeof(fw_version) - 1);
    strncpy(fw_checksum, checksum->valuestring, sizeof(fw_checksum) - 1);
    fw_size = (uint32_t)size->valueint;

    cJSON_Delete(root);

    memset(&g_ota_info, 0, sizeof(g_ota_info));
    strncpy(g_ota_info.fw_title, fw_title, sizeof(g_ota_info.fw_title) - 1);
    strncpy(g_ota_info.fw_version, fw_version, sizeof(g_ota_info.fw_version) - 1);
    strncpy(g_ota_info.fw_checksum, fw_checksum, sizeof(g_ota_info.fw_checksum) - 1);
    g_ota_info.fw_size = fw_size;

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

    memset(g_ota_info.recv_buf, 0, sizeof(g_ota_info.recv_buf));
    memcpy(g_ota_info.recv_buf, data, len);
    g_ota_info.recv_len = len;
    g_ota_info.recv_flag = 1;
}

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

    printf("[OTA] request chunk topic=%s bytes=%s\r\n", topic, payload);

    return App4GPushData(topic, payload);
}

static uint8_t OtaSetFlag(void)
{
    uint8_t flag[4] = OTA_FLAG_MAGIC;
    return DrvEepromWrite(OTA_FLAG_ADDR, flag, 4);
}

static uint8_t OtaDownloadFirmware(void)
{
    uint32_t total_chunks;
    uint32_t crc32_calc;
    uint32_t crc32_flash;
    uint32_t crc32_expect;
    uint32_t crc32_mpeg2;
    uint32_t crc32_expect_swap;
    uint32_t write_offset;
    uint32_t valid_len;
    uint32_t timeout;
    uint32_t last_chunk_len;
    uint32_t chunk_crc;
    uint32_t uart_drop_count;
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

    total_chunks = g_ota_info.fw_size / OTA_CHUNK_SIZE;
    if((g_ota_info.fw_size % OTA_CHUNK_SIZE) != 0U)
    {
        total_chunks++;
    }
    last_chunk_len = g_ota_info.fw_size % OTA_CHUNK_SIZE;
    if(last_chunk_len == 0U)
    {
        last_chunk_len = OTA_CHUNK_SIZE;
    }

    if(OtaParseChecksumHex(g_ota_info.fw_checksum, &crc32_expect) != ESUCCESS)
    {
        printf("[OTA] checksum parse error: %s\r\n", g_ota_info.fw_checksum);
        return EFAIL;
    }

    printf("[OTA] total_chunks=%u last_chunk_len=%u crc_expect=0x%08X\r\n",
           (unsigned int)total_chunks,
           (unsigned int)last_chunk_len,
           (unsigned int)crc32_expect);

    if(FlashFwInit() != ESUCCESS)
    {
        printf("[OTA] flash init error\r\n");
        return EFAIL;
    }

    printf("[OTA] erase download area...\r\n");
    if(FlashFwEraseDownloadSize(g_ota_info.fw_size) != ESUCCESS)
    {
        printf("[OTA] erase error\r\n");
        return EFAIL;
    }

    crc32_calc = CRC32_INIT_VALUE;
    g_ota_info.request_id = BspGetTick() & 0xFFFFU;
    g_ota_info.chunk_id = 0;
    g_ota_info.received_bytes = 0;
    g_ota_info.request_bytes = OTA_CHUNK_SIZE;
    g_ota_info.recv_len = 0;

    for(uint32_t i = 0; i < total_chunks; i++)
    {
        /*
         * ThingsBoard uses the requested chunk size to calculate chunk offset.
         * Keep the request size fixed, and only trim the valid length locally
         * for the last partial chunk.
         */
        g_ota_info.request_bytes = OTA_CHUNK_SIZE;

        g_ota_info.recv_flag = 0;
        g_ota_info.recv_len = 0;
        memset(g_ota_info.recv_buf, 0, sizeof(g_ota_info.recv_buf));

        if(OtaSendChunkRequest() != ESUCCESS)
        {
            printf("[OTA] send chunk %u request error\r\n", (unsigned int)g_ota_info.chunk_id);
            return EFAIL;
        }

        timeout = OTA_RECV_TIMEOUT_MS;
        while(g_ota_info.recv_flag == 0)
        {
            App4GProcess();
            Delay_ms(1);
            timeout--;
            if(timeout == 0U)
            {
                printf("[OTA] chunk %u timeout\r\n", (unsigned int)g_ota_info.chunk_id);
                return EFAIL;
            }
        }

        if((i == (total_chunks - 1U)) && ((g_ota_info.fw_size % OTA_CHUNK_SIZE) != 0U))
        {
            valid_len = g_ota_info.fw_size % OTA_CHUNK_SIZE;
        }
        else
        {
            valid_len = OTA_CHUNK_SIZE;
        }

        if(g_ota_info.recv_len < valid_len || g_ota_info.recv_len > OTA_CHUNK_SIZE)
        {
            printf("[OTA] chunk %u len mismatch recv=%u valid=%u request=%u\r\n",
                   (unsigned int)g_ota_info.chunk_id,
                   (unsigned int)g_ota_info.recv_len,
                   (unsigned int)valid_len,
                   (unsigned int)g_ota_info.request_bytes);
            return EFAIL;
        }

        write_offset = g_ota_info.chunk_id * OTA_CHUNK_SIZE;
        if(g_ota_info.chunk_id == 0U)
        {
            OtaPrintHex("[OTA] chunk0 first16:", g_ota_info.recv_buf, 16U);
        }

        chunk_crc = ComponentCRC32Calc(g_ota_info.recv_buf, valid_len);

        if(FlashFwWriteDownload(write_offset, g_ota_info.recv_buf, valid_len) != ESUCCESS)
        {
            printf("[OTA] write chunk %u error\r\n", (unsigned int)g_ota_info.chunk_id);
            return EFAIL;
        }

        if(FlashFwVerifyDownload(write_offset, g_ota_info.recv_buf, valid_len) != ESUCCESS)
        {
            printf("[OTA] flash verify chunk=%u write_addr=0x%08X len=%u failed\r\n",
                   (unsigned int)g_ota_info.chunk_id,
                   (unsigned int)(FLASH_FW_DOWNLOAD_ADDR + write_offset),
                   (unsigned int)valid_len);
            return EFAIL;
        }

        crc32_calc = ComponentCRC32Update(crc32_calc, g_ota_info.recv_buf, valid_len);
        g_ota_info.received_bytes += valid_len;
        uart_drop_count = Drv4GUARTGetDropCount();

        printf("[OTA] chunk_id=%u recv_len=%u chunk_crc=0x%08X uart_drop=%u write_addr=0x%08X\r\n",
               (unsigned int)g_ota_info.chunk_id,
               (unsigned int)valid_len,
               (unsigned int)chunk_crc,
               (unsigned int)uart_drop_count,
               (unsigned int)(FLASH_FW_DOWNLOAD_ADDR + write_offset));

        g_ota_info.chunk_id++;
    }

    crc32_calc ^= CRC32_XOR_VALUE;
    crc32_flash = FlashFwCalcDownloadCRC32(g_ota_info.fw_size);
    crc32_mpeg2 = FlashFwCalcDownloadCRC32Mpeg2(g_ota_info.fw_size);
    crc32_expect_swap = OtaSwapU32(crc32_expect);

    FlashFwDebugDump(0U, 32U);

    printf("[OTA] fw_size=%u received_bytes=%u total_chunks=%u last_chunk_len=%u crc_calc=0x%08X crc_flash=0x%08X crc_expect=0x%08X\r\n",
           (unsigned int)g_ota_info.fw_size,
           (unsigned int)g_ota_info.received_bytes,
           (unsigned int)total_chunks,
           (unsigned int)last_chunk_len,
           (unsigned int)crc32_calc,
           (unsigned int)crc32_flash,
           (unsigned int)crc32_expect);

    printf("[OTA] crc_diag ieee=0x%08X ieee_swap=0x%08X ieee_no_xor=0x%08X mpeg2=0x%08X expect=0x%08X\r\n",
           (unsigned int)crc32_calc,
           (unsigned int)OtaSwapU32(crc32_calc),
           (unsigned int)(crc32_calc ^ CRC32_XOR_VALUE),
           (unsigned int)crc32_mpeg2,
           (unsigned int)crc32_expect);

    if(crc32_calc != crc32_flash)
    {
        printf("[OTA] flash verify mismatch! stream=0x%08X flash=0x%08X\r\n",
               (unsigned int)crc32_calc,
               (unsigned int)crc32_flash);
        return EFAIL;
    }

    if(crc32_calc != crc32_expect && crc32_calc != crc32_expect_swap)
    {
        printf("[OTA] crc32 mismatch!\r\n");
        return EFAIL;
    }

    if(crc32_calc == crc32_expect_swap && crc32_calc != crc32_expect)
    {
        printf("[OTA] crc32 matched by swapped platform checksum: expect=0x%08X swap=0x%08X\r\n",
               (unsigned int)crc32_expect,
               (unsigned int)crc32_expect_swap);
    }

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

    if(OtaSetFlag() != ESUCCESS)
    {
        printf("[OTA] set eeprom flag error\r\n");
        return EFAIL;
    }

    {
        uint8_t buf[32];
        uint32_t fw_size = g_ota_info.fw_size;

        memset(buf, 0, sizeof(buf));
        strncpy((char *)buf, g_ota_info.fw_title, sizeof(buf) - 1);
        DrvEepromWrite(OTA_TITLE_ADDR, buf, sizeof(buf));

        memset(buf, 0, sizeof(buf));
        strncpy((char *)buf, g_ota_info.fw_version, sizeof(buf) - 1);
        DrvEepromWrite(OTA_VERSION_ADDR, buf, sizeof(buf));

        memset(buf, 0, sizeof(buf));
        strncpy((char *)buf, g_ota_info.fw_checksum, sizeof(buf) - 1);
        DrvEepromWrite(OTA_CHECKSUM_ADDR, buf, sizeof(buf));

        DrvEepromWrite(OTA_SIZE_ADDR, (uint8_t *)&fw_size, 4);
    }

    printf("[OTA] eeprom: title=%s ver=%s size=%u checksum=%s\r\n",
           g_ota_info.fw_title,
           g_ota_info.fw_version,
           (unsigned int)g_ota_info.fw_size,
           g_ota_info.fw_checksum);
    printf("[OTA] rebooting...\r\n");

    App4GPushData(APP_4G_PUB_TELEMETRY, "{\"fw_state\":\"UPDATED\"}");

    Delay_ms(500);
    NVIC_SystemReset();

    return ESUCCESS;
}

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
