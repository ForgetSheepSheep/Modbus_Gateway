#include "boot_update.h"
#include "boot_jump.h"
#include "component_ymodem/component_ymodem.h"
#include "component_flash_fw/component_flash_fw.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_delay/driver_delay.h"
#include "./driver_uart/driver_uart.h"
#include "./driver_gd25q128/driver_gd25q128.h"
#include "./driver_flash/driver_flash.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

/* YMODEM组件句柄 */
static YmodemHandle_t g_ymodem_handle;

 
/************************************************************
* @brief YMODEM串口接收1字节回调函数
* @param pdata      接收数据保存地址
* @param timeout_ms 接收超时时间
* @return 0表示接收成功，非0表示接收失败
************************************************************/
static int32_t BootUpdateRecvByte(uint8_t *pdata, uint32_t timeout_ms)
{
    uint32_t start = BspGetTick();

    while((BspGetTick() - start) < timeout_ms)
    {
        if(DrvUARTReadByte(pdata) == 0)
        {
            return 0;
        }
    }

    return -1;
}

/************************************************************
* @brief YMODEM串口发送1字节回调函数
* @param data 要发送的数据
* @return 无
************************************************************/
static void BootUpdateSendByte(uint8_t data)
{
   DrvUARTSendData(UART_ID_0, &data, 1);
}

/************************************************************
* @brief YMODEM写固件数据回调函数
* @param offset 当前数据相对于固件起始位置的偏移
* @param pdata  数据指针
* @param len    数据长度
* @return YMODEM_RET_OK表示写入成功
* @note  此处不写内部Flash，只写入GD25Q128外部Flash下载区
************************************************************/
static YmodemRet_e BootUpdateWriteData(uint32_t offset, uint8_t *pdata, uint32_t len)
{
    if(pdata == NULL)
    {
        return YMODEM_RET_ERROR;
    }

    if(len == 0)
    {
        return YMODEM_RET_OK;
    }

    if(FlashFwWriteDownload(offset, pdata, len) != ESUCCESS)
    {
        return YMODEM_RET_ERROR;
    }

    return YMODEM_RET_OK;
}

/************************************************************
* @brief 通过YMODEM接收固件，并保存到GD25Q128外部Flash下载区
* @param 无
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  流程：初始化外部Flash -> 擦除下载区 -> YMODEM接收 -> 计算CRC32 -> 写入固件信息头
************************************************************/
uint8_t BootUpdateByYmodem(void)
{
    YmodemRet_e ret;
    FlashFwInfo_t fw_info;
    uint32_t calc_crc32;

    printf("BOOT UPDATE BY YMODEM\r\n");

    /* 1. 初始化外部Flash固件管理模块 */
    if(FlashFwInit() != ESUCCESS)
    {
        printf("FlashFw Init Error\r\n");
        return EFAIL;
    }

    printf("FlashFw Init OK\r\n");

    /* 2. 擦除外部Flash固件下载区 */
    if(FlashFwEraseDownloadArea() != ESUCCESS)
    {
        printf("Erase Download Area Error\r\n");
        return EFAIL;
    }

    printf("Erase Download Area OK\r\n");

    /* 3. 清空串口接收缓冲区，避免残留数据干扰YMODEM */
    {
        uint8_t tmp;
        Delay_ms(100);
        while(DrvUARTReadByte(&tmp) == 0);
    }

    /* 4. 初始化YMODEM组件 */
    ComponentYmodemInit(&g_ymodem_handle,
                        BootUpdateRecvByte,
                        BootUpdateSendByte,
                        BootUpdateWriteData);

    /* 4. 开始接收SecureCRT发送的bin文件 */
    ret = ComponentYmodemReceive(&g_ymodem_handle);

    if(ret != YMODEM_RET_OK)
    {
        printf("YMODEM Receive Error: %d\r\n", ret);
        return EFAIL;
    }

    printf("YMODEM Receive OK\r\n");
    printf("File Name: %s\r\n", g_ymodem_handle.file_info.file_name);
    printf("File Size: %lu\r\n", g_ymodem_handle.file_info.file_size);
    printf("Recv Size: %lu\r\n", g_ymodem_handle.file_info.recv_size);

    /* 5. 检查接收大小是否正确 */
    if(g_ymodem_handle.file_info.file_size == 0)
    {
        printf("File Size Error\r\n");
        return EFAIL;
    }

    if(g_ymodem_handle.file_info.recv_size != g_ymodem_handle.file_info.file_size)
    {
        printf("Recv Size Error\r\n");
        return EFAIL;
    }

    /* 6. 计算外部Flash下载区中的固件CRC32 */
    calc_crc32 = FlashFwCalcDownloadCRC32(g_ymodem_handle.file_info.file_size);

    if(calc_crc32 == 0)
    {
        printf("Calc CRC32 Error\r\n");
        return EFAIL;
    }

    printf("Calc CRC32: 0x%08lX\r\n", calc_crc32);

    /* 7. 写入固件信息头 */
    memset(&fw_info, 0, sizeof(FlashFwInfo_t));

    fw_info.fw_size    = g_ymodem_handle.file_info.file_size;
    fw_info.fw_crc32   = calc_crc32;
    fw_info.fw_version = 1;
    fw_info.fw_addr    = FLASH_FW_DOWNLOAD_ADDR;

    if(FlashFwWriteInfo(&fw_info) != ESUCCESS)
    {
        printf("FlashFw Write Info Error\r\n");
        return EFAIL;
    }

    printf("FlashFw Write Info OK\r\n");
    printf("UPDATE SAVE TO EXFLASH OK\r\n");

    return ESUCCESS;
}

/************************************************************
* @brief 获取YMODEM接收到的固件文件大小
* @return 固件文件大小（字节），未接收时返回0
************************************************************/
uint32_t BootUpdateGetFwSize(void)
{
    return g_ymodem_handle.file_info.file_size;
}

/************************************************************
* @brief 从GD25Q128搬运固件到内部Flash APP区
* @param fw_size 固件大小（字节）
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  流程：擦除内部APP区 -> 从GD25Q128读取 -> 写入内部Flash -> 校验APP有效性
************************************************************/
uint8_t BootCopyExtFlashToApp(uint32_t fw_size)
{
    uint8_t buf[256];
    uint32_t offset = 0;
    uint32_t read_len;

    if(fw_size == 0)
    {
        return EFAIL;
    }

    printf("[FLASH] erase app area...\r\n");

    if(DrvFlashErase(APP_START_ADDR, fw_size) != 0)
    {
        printf("[FLASH] erase app error\r\n");
        return EFAIL;
    }

    printf("[FLASH] erase app ok\r\n");
    printf("[FLASH] copy firmware...\r\n");

    while(offset < fw_size)
    {
        if((fw_size - offset) > sizeof(buf))
        {
            read_len = sizeof(buf);
        }
        else
        {
            read_len = fw_size - offset;
        }

        if(DrvGD25Q128ReadBuf(FLASH_FW_DOWNLOAD_ADDR + offset, buf, read_len) != ESUCCESS)
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

    printf("[FLASH] copy firmware ok\r\n");

    if(BootCheckAppValid(APP_START_ADDR) != 1)
    {
        printf("NEW APP INVALID\r\n");
        return EFAIL;
    }

    printf("NEW APP VALID\r\n");

    return ESUCCESS;
}
