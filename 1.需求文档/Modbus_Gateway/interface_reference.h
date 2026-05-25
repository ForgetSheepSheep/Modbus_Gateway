/**
 * @file  interface_reference.h
 * @brief Bootloader 项目已实现接口汇总（仅作查阅，不参与编译）
 */

#ifndef __INTERFACE_REFERENCE_H
#define __INTERFACE_REFERENCE_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 *  公共定义
 * ============================================================ */
#define ESUCCESS    0
#define EFAIL       1

#define OPEN        1
#define CLOSE       0

/* ============================================================
 *  BSP 层
 * ============================================================ */

/* bsp_tick —— SysTick 1ms 系统节拍 */
void     BspTickInit(void);
uint32_t BspGetTick(void);
void     BspCallbackRegister(void (*fun)(void));

/* ============================================================
 *  Driver 层
 * ============================================================ */

/* ---------- driver_delay ---------- */
void DrvDelayInit(void);
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);

/* ---------- driver_uart ---------- */
typedef enum {
    UART_ID_0 = 0,
    UART_ID_MAX
} UARTId_e;

void    DrvUARTInit(uint32_t baudrate);
void    DrvUARTSendData(uint8_t uart_id, uint8_t *pdata, uint16_t len);
void    DrvUARTSendString(uint8_t uart_id, char *str);
uint8_t DrvUARTReadByte(uint8_t *pdata);

/* ---------- driver_led ---------- */
typedef enum {
    LED_ID_1 = 0,
    LED_ID_2,
    LED_ID_3,
    LED_ID_MAX
} LedId_e;

void DrvLedInit(void);
void DrvLedCtrl(uint8_t led_id, uint8_t led_status);

/* ---------- driver_spi ---------- */
void    DrvSPIInit(void);
uint8_t DrvSPIReadWriteByte(uint8_t tx_data);

/* ---------- driver_gd25q128 ---------- */
#define GD25Q128_JEDEC_ID   0xC84018UL

void     DrvGD25Q128Init(void);
uint32_t DrvGD25Q128ReadID(void);
void     DrvGD25Q128WriteEnable(void);
void     DrvGD25Q128WriteDisable(void);
uint8_t  DrvGD25Q128ReadStatus(void);
void     DrvGD25Q128WaitBusy(void);
uint8_t  DrvGD25Q128SectorErase(uint32_t addr);
uint8_t  DrvGD25Q128BlockErase64K(uint32_t addr);
uint8_t  DrvGD25Q128PageWrite(uint32_t addr, uint8_t *buf, uint16_t len);
uint8_t  DrvGD25Q128WriteBuf(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t  DrvGD25Q128ReadBuf(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t  DrvGD25Q128EraseArea(uint32_t addr, uint32_t len);

/* ---------- driver_flash（内部 Flash） ---------- */
int32_t DrvFlashErase(uint32_t addr, uint32_t size);
int32_t DrvFlashWrite(uint32_t addr, uint8_t *buf, uint32_t len);

/* ============================================================
 *  Component 层
 * ============================================================ */

/* ---------- ring_buffer ---------- */
typedef struct {
    uint8_t           *pbuffer;
    volatile uint16_t  buf_size;
    volatile uint16_t  pw;
    volatile uint16_t  pr;
} RingBuffer_t;

uint8_t RingBufferInit(RingBuffer_t *pdst_buf, uint8_t *buffer, uint16_t buf_size);
uint8_t RingBufferWrite(RingBuffer_t *pdst_buf, uint8_t byte);
uint8_t RingBufferRead(RingBuffer_t *pdst_buf, uint8_t *byte);

/* ---------- component_flash_fw（外部 Flash 固件管理） ---------- */
#define FLASH_FW_DOWNLOAD_ADDR   0x000000UL
#define FLASH_FW_AREA_SIZE       (512UL * 1024UL)
#define FLASH_FW_INFO_ADDR       0x0F0000UL
#define FLASH_FW_INFO_MAGIC      0x46574D47UL

typedef struct {
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
uint32_t FlashFwCalcDownloadCRC32(uint32_t fw_size);
uint8_t  FlashFwWriteInfo(FlashFwInfo_t *info);
uint8_t  FlashFwReadInfo(FlashFwInfo_t *info);
uint8_t  FlashFwCheckInfo(FlashFwInfo_t *info);

/* ---------- component_ymodem ---------- */
typedef enum {
    YMODEM_RET_OK = 0,
    YMODEM_RET_ERROR,
    YMODEM_RET_TIMEOUT,
    YMODEM_RET_CANCEL,
    YMODEM_RET_CRC_ERROR,
    YMODEM_RET_PACKET_ERROR
} YmodemRet_e;

typedef struct {
    char     file_name[64];
    uint32_t file_size;
    uint32_t recv_size;
} YmodemFileInfo_t;

typedef YmodemRet_e (*YmodemWriteCallback_t)(uint32_t offset, uint8_t *pdata, uint32_t len);
typedef int32_t     (*YmodemRecvByteCallback_t)(uint8_t *pdata, uint32_t timeout_ms);
typedef void        (*YmodemSendByteCallback_t)(uint8_t data);

typedef struct {
    YmodemRecvByteCallback_t recv_byte;
    YmodemSendByteCallback_t send_byte;
    YmodemWriteCallback_t    write_data;
    YmodemFileInfo_t         file_info;
} YmodemHandle_t;

void        ComponentYmodemInit(YmodemHandle_t *handle,
                                YmodemRecvByteCallback_t recv_byte,
                                YmodemSendByteCallback_t send_byte,
                                YmodemWriteCallback_t write_data);
YmodemRet_e ComponentYmodemReceive(YmodemHandle_t *handle);

/* ============================================================
 *  Boot 层
 * ============================================================ */

/* ---------- boot_jump ---------- */
#define APP_START_ADDR   0x08004000U

uint8_t BootCheckAppValid(uint32_t app_addr);
void    BootJumpToApp(uint32_t app_addr);

/* ---------- boot_update ---------- */
#define BOOT_APP_START_ADDR   0x08004000U
#define BOOT_APP_MAX_SIZE     (256 * 1024U)

uint8_t  BootUpdateByYmodem(void);
uint32_t BootUpdateGetFwSize(void);
uint8_t  BootCopyExtFlashToApp(uint32_t fw_size);

/* ---------- boot_main ---------- */
void BootMainInit(void);
void BootMainProcess(void);

#endif /* __INTERFACE_REFERENCE_H */
