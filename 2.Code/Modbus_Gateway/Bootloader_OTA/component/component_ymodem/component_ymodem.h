#ifndef __COMPONENT_YMODEM_H
#define __COMPONENT_YMODEM_H

#include <stdint.h>

/* YMODEM控制字符 */
#define YMODEM_SOH                 0x01    /* 128字节数据包 */
#define YMODEM_STX                 0x02    /* 1024字节数据包 */
#define YMODEM_EOT                 0x04    /* 传输结束 */
#define YMODEM_ACK                 0x06    /* 应答成功 */
#define YMODEM_NAK                 0x15    /* 应答失败 */
#define YMODEM_CAN                 0x18    /* 取消传输 */
#define YMODEM_CRC                 0x43    /* 字符'C'，表示使用CRC16校验 */

/* YMODEM数据长度 */
#define YMODEM_PACKET_128_SIZE     128
#define YMODEM_PACKET_1K_SIZE      1024
#define YMODEM_PACKET_OVERHEAD     5       /* 包头 + 包号 + 包号反码 + CRC16 */
#define YMODEM_PACKET_MAX_SIZE     (YMODEM_PACKET_1K_SIZE + YMODEM_PACKET_OVERHEAD)

/* YMODEM超时和重试 */
#define YMODEM_RX_TIMEOUT_MS       1000
#define YMODEM_RETRY_MAX           30

/* YMODEM接收状态 */
typedef enum
{
    YMODEM_RET_OK = 0,
    YMODEM_RET_ERROR,
    YMODEM_RET_TIMEOUT,
    YMODEM_RET_CANCEL,
    YMODEM_RET_CRC_ERROR,
    YMODEM_RET_PACKET_ERROR,

} YmodemRet_e;

/* YMODEM文件信息 */
typedef struct
{
    char file_name[64];          /* 文件名 */
    uint32_t file_size;          /* 文件大小 */
    uint32_t recv_size;          /* 已经接收的大小 */

} YmodemFileInfo_t;

/* YMODEM写数据回调函数 */
typedef YmodemRet_e (*YmodemWriteCallback_t)(uint32_t offset, uint8_t *pdata, uint32_t len);

/* YMODEM接收字节函数 */
typedef int32_t (*YmodemRecvByteCallback_t)(uint8_t *pdata, uint32_t timeout_ms);

/* YMODEM发送字节函数 */
typedef void (*YmodemSendByteCallback_t)(uint8_t data);

/* YMODEM组件对象 */
typedef struct
{
    YmodemRecvByteCallback_t recv_byte;     /* 串口接收1字节 */
    YmodemSendByteCallback_t send_byte;     /* 串口发送1字节 */
    YmodemWriteCallback_t    write_data;    /* 写Flash回调 */

    YmodemFileInfo_t file_info;             /* 文件信息 */

} YmodemHandle_t;

void ComponentYmodemInit(YmodemHandle_t *handle,
                         YmodemRecvByteCallback_t recv_byte,
                         YmodemSendByteCallback_t send_byte,
                         YmodemWriteCallback_t write_data);

YmodemRet_e ComponentYmodemReceive(YmodemHandle_t *handle);

#endif /* __COMPONENT_YMODEM_H */
