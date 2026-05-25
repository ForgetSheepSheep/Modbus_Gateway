#include "./driver_key/driver_key.h"
#include "./bsp_tick/bsp_tick.h"
#include "./ring_buffer/ring_buffer.h"


#define KEY_LEVEL_RELEASE        0       /* 逻辑松开状态 */
#define KEY_LEVEL_PRESS          1       /* 逻辑按下状态 */

#define KEY_CONFIRM_TIME_MS      10      /* 按键消抖确认时间 */
#define KEY_DOUBLE_TIME_MS       300     /* 双击最大间隔时间 */
#define KEY_LONG_PRESS_TIME_MS   800     /* 长按判定时间 */

#define KEY_RING_BUF_SIZE        32      /* 按键事件环形缓冲区大小 */


/************************************************************
* @brief KEY GPIO信息结构体
* @note  用来统一保存一个KEY对应的GPIO时钟、端口和引脚
************************************************************/
typedef struct
{
    rcu_periph_enum gpio_rcu;      /* GPIO外设时钟，例如RCU_GPIOA */
    uint32_t gpio_port;            /* GPIO端口，例如GPIOA */
    uint32_t gpio_pin;             /* GPIO引脚，例如GPIO_PIN_0 */

} KeyGPIO_t;


/* KEY GPIO配置表，后续增加按键只需要在这里继续添加 */
static const KeyGPIO_t g_key_gpio_table[] =
{
    {.gpio_rcu = RCU_GPIOA, .gpio_port = GPIOA, .gpio_pin = GPIO_PIN_0 },   /* KEY1：PA0  */
    {.gpio_rcu = RCU_GPIOG, .gpio_port = GPIOG, .gpio_pin = GPIO_PIN_13},   /* KEY2：PG13 */
    {.gpio_rcu = RCU_GPIOG, .gpio_port = GPIOG, .gpio_pin = GPIO_PIN_14},   /* KEY3：PG14 */
    {.gpio_rcu = RCU_GPIOG, .gpio_port = GPIOG, .gpio_pin = GPIO_PIN_15},   /* KEY4：PG15 */
};


#define KEY_NUM_MAX    (sizeof(g_key_gpio_table) / sizeof(g_key_gpio_table[0]))  /* KEY数量 */


/************************************************************
* @brief 按键内部状态枚举
* @note  用来表示按键扫描过程中的内部判断状态
************************************************************/
typedef enum
{
    KEY_STATE_IDLE = 0,               /* 空闲状态，等待按键按下 */
    KEY_STATE_DEBOUNCE_PRESS,         /* 按下消抖状态 */
    KEY_STATE_PRESS_CONFIRM,          /* 已确认按下状态 */
    KEY_STATE_WAIT_DOUBLE_PRESS,      /* 等待第二次按下状态 */
    KEY_STATE_DEBOUNCE_SECOND_PRESS,  /* 第二次按下消抖状态 */
    KEY_STATE_WAIT_RELEASE,           /* 等待按键松开状态 */

} key_state_t;


/************************************************************
* @brief 按键状态管理结构体
* @note  用来保存每一个按键状态机运行过程中的状态和时间信息
************************************************************/
typedef struct
{
    key_state_t key_state;          /* 当前按键状态机状态 */
    uint32_t first_io_change_tick;   /* 第一次检测到IO变化的时间，用于消抖 */
    uint32_t first_press_tick;      /* 第一次确认按下的时间，用于判断长按 */
    uint32_t first_release_tick;    /* 第一次确认松开的时间，用于判断双击间隔 */

} KeyInfo_t;


static KeyInfo_t g_keyinfo[KEY_NUM_MAX];                        /* 每个按键各自的状态机信息 */
static RingBuffer_t g_key_ring_buffer;                          /* 按键事件环形缓冲区管理结构体 */
static uint8_t g_key_ring_buf[KEY_RING_BUF_SIZE] = {0};         /* 按键事件环形缓冲区实际存储空间 */


/************************************************************
* @brief KEY驱动初始化
* @param 无
* @return 无
************************************************************/
void DrvKeyInit(void)
{
    uint8_t i = 0;     /* 循环变量，用来遍历KEY配置表 */

    RingBufferInit(&g_key_ring_buffer, g_key_ring_buf, KEY_RING_BUF_SIZE);     /* 初始化按键事件环形缓冲区 */

    for(i = 0; i < KEY_NUM_MAX; i++)
    {
        rcu_periph_clock_enable(g_key_gpio_table[i].gpio_rcu);    /* 使能当前KEY对应的GPIO时钟 */

        gpio_init(g_key_gpio_table[i].gpio_port,                  /* 当前KEY对应的GPIO端口 */
                  GPIO_MODE_IPU,                                  /* 配置为上拉输入模式，默认高电平 */
                  GPIO_OSPEED_10MHZ,                              /* GPIO速度配置为10MHz */
                  g_key_gpio_table[i].gpio_pin);                  /* 当前KEY对应的GPIO引脚 */
    }
}


/************************************************************
* @brief 获取指定按键的逻辑状态
* @param index 按键下标
* @return KEY_LEVEL_PRESS：按下，KEY_LEVEL_RELEASE：松开
************************************************************/
static uint8_t KeyGetLevel(uint8_t index)
{
    uint8_t io_level = 0;     /* 保存GPIO实际读取到的电平 */

    io_level = gpio_input_bit_get(g_key_gpio_table[index].gpio_port,
                                  g_key_gpio_table[index].gpio_pin);    /* 读取按键GPIO电平 */

    if(io_level)
    {
        return KEY_LEVEL_RELEASE;    /* 上拉输入，读到高电平表示按键松开 */
    }
    else
    {
        return KEY_LEVEL_PRESS;      /* 上拉输入，读到低电平表示按键按下 */
    }
}


/************************************************************
* @brief 单个按键扫描函数
* @param index 按键下标
* @return 当前按键事件
************************************************************/
static KeyEvent_t KeyScanOne(uint8_t index)
{
    uint8_t ispress = KEY_LEVEL_RELEASE;       /* 当前按键逻辑状态 */
    uint32_t now_tick = 0;                     /* 当前系统时间 */

    if(index >= KEY_NUM_MAX)
    {
        return KEY_EVENT_NONE;                 /* 按键下标非法，直接返回无事件 */
    }

    ispress = KeyGetLevel(index);              /* 获取当前按键是按下还是松开 */
    now_tick = BspGetTick();                   /* 获取当前系统tick */

    switch(g_keyinfo[index].key_state)
    {
        case KEY_STATE_IDLE:
        {
            if(ispress == KEY_LEVEL_PRESS)
            {
                g_keyinfo[index].first_io_change_tick = now_tick;             /* 记录第一次检测到按下的时间 */
                g_keyinfo[index].key_state = KEY_STATE_DEBOUNCE_PRESS;      /* 进入按下消抖状态 */
            }
            break;
        }

        case KEY_STATE_DEBOUNCE_PRESS:
        {
            if(ispress == KEY_LEVEL_PRESS)
            {
                if(now_tick - g_keyinfo[index].first_io_change_tick >= KEY_CONFIRM_TIME_MS)
                {
                    g_keyinfo[index].first_press_tick = now_tick;            /* 记录确认按下的时间 */
                    g_keyinfo[index].key_state = KEY_STATE_PRESS_CONFIRM;   /* 进入确认按下状态 */
                }
            }
            else
            {
                g_keyinfo[index].key_state = KEY_STATE_IDLE;                /* 消抖期间松开，说明是抖动，回到空闲 */
            }
            break;
        }

        case KEY_STATE_PRESS_CONFIRM:
        {
            if(ispress == KEY_LEVEL_PRESS)
            {
                if(now_tick - g_keyinfo[index].first_press_tick >= KEY_LONG_PRESS_TIME_MS)
                {
                    g_keyinfo[index].key_state = KEY_STATE_WAIT_RELEASE;    /* 长按事件只返回一次，后面等松手 */
                    return KEY_EVENT_LONG_PRESS;                           /* 返回长按事件 */
                }
            }
            else
            {
                g_keyinfo[index].first_release_tick = now_tick;              /* 记录第一次松开的时间 */
                g_keyinfo[index].key_state = KEY_STATE_WAIT_DOUBLE_PRESS;   /* 进入等待双击状态 */
            }
            break;
        }

        case KEY_STATE_WAIT_DOUBLE_PRESS:
        {
            if(ispress == KEY_LEVEL_PRESS)
            {
                g_keyinfo[index].first_io_change_tick = now_tick;                 /* 记录第二次按下的时间 */
                g_keyinfo[index].key_state = KEY_STATE_DEBOUNCE_SECOND_PRESS;   /* 进入第二次按下消抖 */
            }
            else
            {
                if(now_tick - g_keyinfo[index].first_release_tick >= KEY_DOUBLE_TIME_MS)
                {
                    g_keyinfo[index].key_state = KEY_STATE_IDLE;                /* 超过双击时间，没有第二次按下，回到空闲 */
                    return KEY_EVENT_SHORT_PRESS;                              /* 返回短按事件 */
                }
            }
            break;
        }

        case KEY_STATE_DEBOUNCE_SECOND_PRESS:
        {
            if(ispress == KEY_LEVEL_PRESS)
            {
                if(now_tick - g_keyinfo[index].first_io_change_tick >= KEY_CONFIRM_TIME_MS)
                {
                    g_keyinfo[index].key_state = KEY_STATE_WAIT_RELEASE;        /* 双击事件只返回一次，后面等松手 */
                    return KEY_EVENT_DOUBLE_PRESS;                             /* 返回双击事件 */
                }
            }
            else
            {
                g_keyinfo[index].key_state = KEY_STATE_WAIT_DOUBLE_PRESS;       /* 第二次按下消抖失败，继续等待双击 */
            }
            break;
        }

        case KEY_STATE_WAIT_RELEASE:
        {
            if(ispress == KEY_LEVEL_RELEASE)
            {
                g_keyinfo[index].key_state = KEY_STATE_IDLE;                    /* 等到按键松开后，重新回到空闲状态 */
            }
            break;
        }

        default:
        {
            g_keyinfo[index].key_state = KEY_STATE_IDLE;                        /* 状态异常时，恢复到空闲状态 */
            break;
        }
    }

    return KEY_EVENT_NONE;                                                     /* 默认没有事件 */
}


/************************************************************
* @brief 按键事件写入环形缓冲区
* @param key_id 按键编号
* @param key_event 按键事件
* @return 无
************************************************************/
static void KeyWriteValue(KeyId_t key_id, KeyEvent_t key_event)
{
    uint8_t key_data = 0;       /* 用1个字节保存按键编号和按键事件 */

    if(key_id >= KEY_ID_MAX)
    {
        return;
    }

    if(key_event == KEY_EVENT_NONE)
    {
        return;
    }

    key_data = ((uint8_t)key_id << 4) | ((uint8_t)key_event & 0x0F);  /* 高4位保存按键编号，低4位保存按键事件 */

    RingBufferWrite(&g_key_ring_buffer, key_data);                  /* 将按键事件写入环形缓冲区 */
}


/************************************************************
* @brief 按键扫描函数
* @param 无
* @return 无
************************************************************/
void DrvKeyScan(void)
{
    uint8_t i = 0;                 /* 循环变量，用来遍历所有按键 */
    KeyEvent_t key_event;          /* 保存当前按键扫描出来的事件 */

    for(i = 0; i < KEY_NUM_MAX; i++)
    {
        key_event = KeyScanOne(i);        /* 扫描当前按键 */

        if(key_event != KEY_EVENT_NONE)
        {
            KeyWriteValue((KeyId_t)i, key_event);    /* 有按键事件就写入环形缓冲区 */
        }
    }
}


/************************************************************
* @brief 读取按键事件
* @param 无
* @return 按键事件结果
************************************************************/
KeyValue_t DrvKeyReadValue(void)
{
    uint8_t key_data = 0;          /* 从环形缓冲区读出的原始按键数据 */
    KeyValue_t key_value;          /* 解包后的按键结果 */

    key_value.key_id = KEY_ID_MAX;             /* 默认无效按键 */
    key_value.key_event = KEY_EVENT_NONE;      /* 默认无按键事件 */

    if(RingBufferRead(&g_key_ring_buffer, &key_data) != 0)
    {
        return key_value;                      /* 读取失败，说明没有按键事件 */
    }

    key_value.key_id = (KeyId_t)(key_data >> 4);          /* 取高4位，得到按键编号 */
    key_value.key_event = (KeyEvent_t)(key_data & 0x0F);  /* 取低4位，得到按键事件 */

    return key_value;
}
