#include "./driver_delay/driver_delay.h"


/* 使用TIMER5作为延时定时器 */
#define DELAY_TIMER             TIMER5
#define DELAY_TIMER_RCU         RCU_TIMER5

/* 
 * GD32F303ZET6 常见配置：
 * 系统主频：120MHz
 * APB1时钟：60MHz
 * APB1定时器时钟：120MHz
 *
 * 所以这里按120MHz计算，让TIMER5计数频率变成1MHz
 * 1MHz = 1us计数一次
 */
#define DELAY_TIMER_CLK_HZ      120000000UL


/************************************************************
* @brief 延时驱动初始化
* @param 无
* @return 无
* @note  使用TIMER5作为自由运行计数器，计数频率配置为1MHz
************************************************************/
void DrvDelayInit(void)
{
    timer_parameter_struct timer_initpara;

    /* 使能TIMER5时钟 */
    rcu_periph_clock_enable(DELAY_TIMER_RCU);

    /* 复位TIMER5 */
    timer_deinit(DELAY_TIMER);

    /* 定时器结构体初始化为默认值 */
    timer_struct_para_init(&timer_initpara);

    /* 
     * 预分频计算：
     * 定时器输入时钟 = 120MHz
     * 目标计数频率 = 1MHz
     * prescaler = 120MHz / 1MHz - 1 = 119
     * 所以TIMER5每计数1次，就是1us
     */
    timer_initpara.prescaler = DELAY_TIMER_CLK_HZ / 1000000UL - 1;

    /* 边沿对齐模式 */
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;

    /* 向上计数 */
    timer_initpara.counterdirection = TIMER_COUNTER_UP;

    /* 最大计数周期 */
    timer_initpara.period = 0xFFFFFFFF;

    /* 不分频 */
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;

    /* 高级定时器才用，普通定时器写0即可 */
    timer_initpara.repetitioncounter = 0;

    /* 初始化TIMER5 */
    timer_init(DELAY_TIMER, &timer_initpara);

    /* 清零计数器 */
    timer_counter_value_config(DELAY_TIMER, 0);

    /* 使能TIMER5 */
    timer_enable(DELAY_TIMER);
}


/************************************************************
* @brief 微秒级延时函数
* @param us 延时时间，单位us
* @return 无
* @note  TIMER5已经配置为1us计数一次
************************************************************/
void Delay_us(uint32_t us)
{
    uint32_t tick_start = 0;

    /* 记录当前计数值 */
    tick_start = TIMER_CNT(DELAY_TIMER);

    /* 等待计数差值达到指定us */
    while((TIMER_CNT(DELAY_TIMER) - tick_start) < us)
    {
        /* 等待 */
    }
}


/************************************************************
* @brief 毫秒级延时函数
* @param ms 延时时间，单位ms
* @return 无
************************************************************/
void Delay_ms(uint32_t ms)
{
    while(ms--)
    {
        Delay_us(1000);
    }
}
