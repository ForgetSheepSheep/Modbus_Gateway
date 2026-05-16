#include "./driver_led/driver_led.h"


/************************************************************
* @brief LED GPIO信息结构体
* @note  用来统一保存一个LED对应的GPIO时钟、端口和引脚
************************************************************/
typedef struct
{
    rcu_periph_enum gpio_rcu;      /* GPIO外设时钟，例如RCU_GPIOA */
    uint32_t gpio_port;            /* GPIO端口，例如GPIOA */
    uint32_t gpio_pin;             /* GPIO引脚，例如GPIO_PIN_8 */

} LedGPIO_t;


/* LED GPIO配置表，后续增加LED只需要在这里继续添加 */
static const LedGPIO_t g_led_gpio_table[] =
{
    {.gpio_rcu = RCU_GPIOA, .gpio_port = GPIOA, .gpio_pin = GPIO_PIN_8},   /* LED0：PA8 */
    {.gpio_rcu = RCU_GPIOE, .gpio_port = GPIOE, .gpio_pin = GPIO_PIN_6},   /* LED1：PE6 */
    {.gpio_rcu = RCU_GPIOF, .gpio_port = GPIOF, .gpio_pin = GPIO_PIN_6},   /* LED2：PF6 */
};


#define LED_NUM_MAX    (sizeof(g_led_gpio_table) / sizeof(g_led_gpio_table[0]))  /* LED数量 */


/************************************************************
* @brief LED驱动初始化
* @param 无
* @return 无
************************************************************/
void DrvLedInit(void)
{
    uint8_t i = 0;     /* 循环变量，用来遍历LED配置表 */

    for(i = 0; i < LED_NUM_MAX; i++)
    {
        rcu_periph_clock_enable(g_led_gpio_table[i].gpio_rcu);    /* 使能当前LED对应的GPIO时钟 */

        gpio_init(g_led_gpio_table[i].gpio_port,                  /* 当前LED对应的GPIO端口 */
                  GPIO_MODE_OUT_PP,                               /* 配置为推挽输出模式 */
                  GPIO_OSPEED_2MHZ,                               /* GPIO输出速度配置为2MHz */
                  g_led_gpio_table[i].gpio_pin);                  /* 当前LED对应的GPIO引脚 */

        gpio_bit_write(g_led_gpio_table[i].gpio_port,             /* 当前LED对应的GPIO端口 */
                       g_led_gpio_table[i].gpio_pin,              /* 当前LED对应的GPIO引脚 */
                       RESET);                                    /* 默认输出低电平 */
    }
}


/************************************************************
* @brief 控制指定LED亮灭
* @param led_id LED编号，从0开始，对应g_led_gpio_table表中的下标
* @param led_status LED状态，1输出高电平，0输出低电平
* @return 无
************************************************************/
void DrvLedCtrl(uint8_t led_id, uint8_t led_status)
{
    bit_status state;     /* 保存最终要输出的GPIO电平状态 */

    if(led_id >= LED_NUM_MAX)
    {
        return;
    }

    state = led_status ? SET : RESET;      /* led_status为1输出高电平，为0输出低电平 */

    gpio_bit_write(g_led_gpio_table[led_id].gpio_port,            /* 根据LED编号获取GPIO端口 */
                   g_led_gpio_table[led_id].gpio_pin,             /* 根据LED编号获取GPIO引脚 */
                   state);                                        /* 输出对应电平 */
}

