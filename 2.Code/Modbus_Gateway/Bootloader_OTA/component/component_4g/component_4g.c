#include "component_4g.h"
#include "./driver_uart/driver_uart.h"
#include "app_config.h"

/************************************************************
* @brief  4G模块初始化
* @param  无
* @return 无
* @note   一般用于初始化USART2、清空接收缓存等
************************************************************/
void FourGInit(void)
{
    /* 用户后续自行实现 */
}

/************************************************************
* @brief  清空4G模块接收缓存
* @param  无
* @return 无
************************************************************/
void FourGClearRecvBuf(void)
{
    /* 用户后续自行实现 */
}

/************************************************************
* @brief  发送AT指令
* @param  cmd: AT指令字符串
* @return FOUR_G_OK/FOUR_G_ERROR
************************************************************/
uint8_t FourGSendCmd(const char *cmd)
{
    /* 用户后续自行实现 */
    (void)cmd;
    return FOUR_G_OK;
}

/************************************************************
* @brief  等待指定应答
* @param  expect: 期望出现的字符串
* @param  timeout_ms: 等待超时时间，单位ms
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGWaitResp(const char *expect, uint32_t timeout_ms)
{
    /* 用户后续自行实现 */
    (void)expect;
    (void)timeout_ms;
    return FOUR_G_TIMEOUT;
}

/************************************************************
* @brief  发送AT指令并等待指定应答
* @param  cmd: AT指令字符串
* @param  expect: 期望出现的字符串
* @param  timeout_ms: 等待超时时间，单位ms
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGSendCmdAndWait(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    /* 用户后续自行实现 */
    (void)cmd;
    (void)expect;
    (void)timeout_ms;
    return FOUR_G_TIMEOUT;
}

/************************************************************
* @brief  检测AT通信是否正常
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGCheckAT(void)
{
    /* 建议指令：AT */
    return FourGSendCmdAndWait("AT\r\n", "OK", 1000);
}

/************************************************************
* @brief  关闭模块回显
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGCloseEcho(void)
{
    /* 建议指令：ATE0 */
    return FourGSendCmdAndWait("ATE0\r\n", "OK", 1000);
}

/************************************************************
* @brief  检测SIM卡状态
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGCheckSim(void)
{
    /* 建议指令：AT+CPIN?，期望READY */
    return FourGSendCmdAndWait("AT+CPIN?\r\n", "READY", 3000);
}

/************************************************************
* @brief  查询信号强度
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGCheckSignal(void)
{
    /* 建议指令：AT+CSQ */
    return FourGSendCmdAndWait("AT+CSQ\r\n", "OK", 1000);
}

/************************************************************
* @brief  检测网络附着状态
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGCheckNetAttach(void)
{
    /* 建议指令：AT+CGATT?，期望+CGATT: 1 */
    return FourGSendCmdAndWait("AT+CGATT?\r\n", "+CGATT: 1", 3000);
}

/************************************************************
* @brief  MQTT连接
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
* @note   不同4G模块MQTT AT指令不同，需按模块手册实现
************************************************************/
uint8_t FourGMqttConnect(void)
{
    /* 用户后续自行实现 */
    return FOUR_G_ERROR;
}

/************************************************************
* @brief  MQTT订阅
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGMqttSubscribe(void)
{
    /* 用户后续自行实现 */
    return FOUR_G_ERROR;
}

/************************************************************
* @brief  MQTT发布hello数据
* @param  无
* @return FOUR_G_OK/FOUR_G_ERROR/FOUR_G_TIMEOUT
************************************************************/
uint8_t FourGMqttPublishHello(void)
{
    /* 用户后续自行实现 */
    return FOUR_G_ERROR;
}

/************************************************************
* @brief  MQTT下发数据处理
* @param  无
* @return 无
************************************************************/
void FourGMqttReceiveProcess(void)
{
    /* 用户后续自行实现 */
}
