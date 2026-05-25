#include "./bsp_tick/bsp_tick.h"


static volatile uint32_t g_bsp_tick_count = 0U;      /* 系统毫秒计数 */
static void (*g_bsp_tick_callback)(void) = 0;        /* SysTick回调函数指针 */
static void BspTickCount(void);
/************************************************************
* @brief BSP Tick初始化，配置SysTick为1ms中断一次
* @param 无
* @return 无
************************************************************/
void BspTickInit(void)
{
    /* rcu_clock_freq_get(CK_AHB) 获取AHB时钟频率，例如120MHz */
    /* 除以1000后，表示SysTick每1ms产生一次中断 */
    if(SysTick_Config(rcu_clock_freq_get(CK_AHB) / 1000U))
    {
        while(1);
    }
}
/************************************************************
* @brief 提供TICK
* @param 无
* @return 让tick计数
************************************************************/
static void BspTickCount(void)
{
    g_bsp_tick_count++;
}
/************************************************************
* @brief 获取当前系统tick计数
* @param 无
* @return 当前系统运行时间，单位ms
************************************************************/
uint32_t BspGetTick(void)
{
    return g_bsp_tick_count;
}

/************************************************************
* @brief 注册SysTick回调函数
* @param fun 需要在SysTick中断中执行的回调函数
* @return 无
************************************************************/
void BspCallbackRegister(void (*fun)(void))
{
    g_bsp_tick_callback = fun;
}

/************************************************************
* @brief SysTick中断服务函数，1ms进入一次
* @param 无
* @return 无
************************************************************/
void SysTick_Handler(void)
{
    void (*callback)(void) = 0;

    BspTickCount();
    
    callback = g_bsp_tick_callback;

    if(callback != 0)
    {
        callback();
    }
}

