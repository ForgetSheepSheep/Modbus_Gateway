#ifndef __COMPONENT_4G_H
#define __COMPONENT_4G_H

#include <stdint.h>


/************************************************************
* @brief 4G AT命令执行状态枚举
************************************************************/
typedef enum
{
    COMPONENT_4G_CMD_IDLE = 0,       /* 空闲状态，没有正在执行的AT命令 */
    COMPONENT_4G_CMD_BUSY,           /* 忙状态，已经发送AT命令，正在等待返回 */
    COMPONENT_4G_CMD_OK,             /* 成功状态，收到期望返回 */
    COMPONENT_4G_CMD_TIMEOUT,        /* 超时状态，等待返回超时 */
    COMPONENT_4G_CMD_ERROR           /* 错误状态，参数错误或模块返回错误 */

} Component4GCmdState_t;


/************************************************************
* @brief 4G组件初始化
* @param 无
* @return 无
* @note  清空接收缓存，初始化AT命令状态
************************************************************/
void Component4GInit(void);


/************************************************************
* @brief 4G组件轮询处理函数
* @param 无
* @return 无
* @note  从USART2环形缓冲区读取4G返回数据，保存到组件内部缓存
************************************************************/
void Component4GProcess(void);


/************************************************************
* @brief 清空4G接收缓存
* @param 无
* @return 无
* @note  每次发送新的AT命令前建议先清空缓存，避免旧数据影响判断
************************************************************/
void Component4GClearRxBuffer(void);


/************************************************************
* @brief 4G模块发送字符串
* @param str 要发送的字符串，例如 "AT\r\n"
* @return 无
* @note  本质是通过USART2发送数据给4G模块
************************************************************/
void Component4GSendString(char *str);


/************************************************************
* @brief 检查4G接收缓存中是否包含指定字符串
* @param expect 期望字符串，例如 "OK"、"READY"、"+CGATT: 1"
* @return 1表示找到，0表示未找到
************************************************************/
uint8_t Component4GCheckResponse(char *expect);


/************************************************************
* @brief 启动一条AT命令
* @param cmd 要发送的AT命令，需要自带\r\n，例如 "AT\r\n"
* @param expect 期望返回字符串，例如 "OK"
* @param timeout_ms 超时时间，单位ms
* @return 1表示启动成功，0表示启动失败
* @note  该函数只负责发送一次命令，不会阻塞等待结果
************************************************************/
uint8_t Component4GSendCmdStart(char *cmd, char *expect, uint32_t timeout_ms);


/************************************************************
* @brief AT命令执行过程处理
* @param 无
* @return Component4GCmdState_t 当前命令状态
* @note  需要在主循环中反复调用，用于判断OK、ERROR或超时
************************************************************/
Component4GCmdState_t Component4GSendCmdProcess(void);


/************************************************************
* @brief 获取当前AT命令状态
* @param 无
* @return Component4GCmdState_t 当前命令状态
************************************************************/
Component4GCmdState_t Component4GGetCmdState(void);


/************************************************************
* @brief 判断当前4G组件是否空闲
* @param 无
* @return 1表示空闲，0表示忙
************************************************************/
uint8_t Component4GIsIdle(void);


#endif /* __COMPONENT_4G_H */
