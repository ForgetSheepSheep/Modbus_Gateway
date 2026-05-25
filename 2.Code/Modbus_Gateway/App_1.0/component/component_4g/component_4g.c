#include "./driver_4g_uart/driver_4g_uart.h"
#include "./component_4g/component_4g.h"
#include "./bsp_tick/bsp_tick.h"

#define COMPONENT_4G_RX_BUF_SIZE     512

typedef struct
{
    Component4GCmdState_t state;    /* 当前AT命令状态 */

    char *cmd;                      /* 当前发送的AT命令 */
    char *expect;                   /* 当前等待的期望返回 */

    uint32_t timeout_ms;            /* 超时时间 */
    uint32_t start_tick;            /* 命令开始时间 */
    uint8_t retry_count;            /* 重试次数，暂时可以不用 */

} Component4GCmd_t;


static uint8_t g_4g_rx_buf[COMPONENT_4G_RX_BUF_SIZE];    /* 4G接收缓存 */
static uint16_t g_4g_rx_len;                             /* 当前接收长度 */

static Component4GCmd_t g_4g_cmd;                        /* 4G命令状态管理结构体 */


/************************************************************
* @brief 4G组件初始化
* @param 无
* @return 无
* @note  清空接收缓存，初始化AT命令状态
************************************************************/
void Component4GInit(void)
{
    /* 如果你的板子上PG7是WiFi关闭控制脚，就保留 */
    rcu_periph_clock_enable(RCU_GPIOG);
    gpio_init(GPIOG, GPIO_MODE_OUT_OD, GPIO_OSPEED_10MHZ, GPIO_PIN_7);
    gpio_bit_reset(GPIOG, GPIO_PIN_7);

    memset(g_4g_rx_buf, 0, COMPONENT_4G_RX_BUF_SIZE);
    g_4g_rx_len = 0;

    g_4g_cmd.state = COMPONENT_4G_CMD_IDLE;
    g_4g_cmd.cmd = NULL;
    g_4g_cmd.expect = NULL;
    g_4g_cmd.timeout_ms = 0;
    g_4g_cmd.start_tick = 0;
    g_4g_cmd.retry_count = 0;
}


/************************************************************
* @brief 4G组件轮询处理函数
* @param 无
* @return 无
* @note  从4G串口环形缓冲区读取数据，保存到组件内部缓存
************************************************************/
void Component4GProcess(void)
{
    uint8_t byte = 0;

    while(Drv4GUARTReadByte(&byte) == ESUCCESS)
    {
        if(g_4g_rx_len < (COMPONENT_4G_RX_BUF_SIZE - 1))
        {
            g_4g_rx_buf[g_4g_rx_len] = byte;
            g_4g_rx_len++;
            g_4g_rx_buf[g_4g_rx_len] = '\0';
        }
        else
        {
            /* 缓存满了，防止数组越界 */
            g_4g_rx_len = 0;
            memset(g_4g_rx_buf, 0, COMPONENT_4G_RX_BUF_SIZE);
        }
    }
}


/************************************************************
* @brief 清空4G接收缓存
* @param 无
* @return 无
* @note  每次发送新的AT命令前建议先清空缓存，避免旧数据影响判断
************************************************************/
void Component4GClearRxBuffer(void)
{
    uint8_t temp = 0;

    memset(g_4g_rx_buf, 0, COMPONENT_4G_RX_BUF_SIZE);
    g_4g_rx_len = 0;

    /* 把串口底层环形缓冲区里面残留的数据也读掉 */
    while(Drv4GUARTReadByte(&temp) == ESUCCESS);
}


/************************************************************
* @brief 4G模块发送字符串
* @param str 要发送的字符串，例如 "AT\r\n"
* @return 无
* @note  本质是通过4G串口发送数据给4G模块
************************************************************/
void Component4GSendString(char *str)
{
    if(str == NULL)
    {
        return;
    }

    if(Drv4GUARTSendString(str) != ESUCCESS) { printf("[4G] send failed\r\n"); }
}


/************************************************************
* @brief 检查4G接收缓存中是否包含指定字符串
* @param expect 期望字符串，例如 "OK"、"READY"、"+CGATT: 1"
* @return 1表示找到，0表示未找到
************************************************************/
uint8_t Component4GCheckResponse(char *expect)
{
    if(expect == NULL)
    {
        return 0;
    }

    if(strstr((char *)g_4g_rx_buf, expect) != NULL)
    {
        return 1;
    }

    return 0;
}


/************************************************************
* @brief 启动一条AT命令
* @param cmd 要发送的AT命令，需要自带\r\n，例如 "AT\r\n"
* @param expect 期望返回字符串，例如 "OK"
* @param timeout_ms 超时时间，单位ms
* @return 1表示启动成功，0表示启动失败
* @note  该函数只负责发送一次命令，不会阻塞等待结果
************************************************************/
uint8_t Component4GSendCmdStart(char *cmd, char *expect, uint32_t timeout_ms)
{
    if(cmd == NULL || expect == NULL)
    {
        return 0;
    }

    if(g_4g_cmd.state == COMPONENT_4G_CMD_BUSY)
    {
        return 0;
    }

    Component4GClearRxBuffer();

    g_4g_cmd.cmd = cmd;
    g_4g_cmd.expect = expect;
    g_4g_cmd.timeout_ms = timeout_ms;
    g_4g_cmd.start_tick = BspGetTick();
    g_4g_cmd.retry_count = 0;
    g_4g_cmd.state = COMPONENT_4G_CMD_BUSY;

    Component4GSendString(cmd);

    return 1;
}


/************************************************************
* @brief AT命令执行过程处理
* @param 无
* @return Component4GCmdState_t 当前命令状态
* @note  需要在主循环中反复调用，用于判断OK、ERROR或超时
************************************************************/
Component4GCmdState_t Component4GSendCmdProcess(void)
{
    if(g_4g_cmd.state != COMPONENT_4G_CMD_BUSY)
    {
        return g_4g_cmd.state;
    }

    /* 先读取串口新数据 */
    Component4GProcess();

    /* 判断是否收到期望返回 */
    if(Component4GCheckResponse(g_4g_cmd.expect) == 1)
    {
        g_4g_cmd.state = COMPONENT_4G_CMD_OK;
        return g_4g_cmd.state;
    }

    /* 判断是否收到ERROR */
    if(Component4GCheckResponse("ERROR") == 1)
    {
        g_4g_cmd.state = COMPONENT_4G_CMD_ERROR;
        return g_4g_cmd.state;
    }

    /* 判断是否超时 */
    if((BspGetTick() - g_4g_cmd.start_tick) >= g_4g_cmd.timeout_ms)
    {
        g_4g_cmd.state = COMPONENT_4G_CMD_TIMEOUT;
        return g_4g_cmd.state;
    }

    return g_4g_cmd.state;
}


/************************************************************
* @brief 获取当前AT命令状态
* @param 无
* @return Component4GCmdState_t 当前命令状态
************************************************************/
Component4GCmdState_t Component4GGetCmdState(void)
{
    return g_4g_cmd.state;
}


/************************************************************
* @brief 判断当前4G组件是否空闲
* @param 无
* @return 1表示空闲，0表示忙，2表示异常状态
************************************************************/
uint8_t Component4GIsIdle(void)
{
    if(g_4g_cmd.state == COMPONENT_4G_CMD_BUSY)
    {
        return 0;
    }
    else if(g_4g_cmd.state == COMPONENT_4G_CMD_IDLE)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}
