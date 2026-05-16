#include "app_task.h"
#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_uart/driver_uart.h"
#include "./driver_led/driver_led.h"
#include "./driver_key/driver_key.h"

/************************************************************
* @brief APP任务函数指针类型
************************************************************/
typedef void (*AppTaskFunc_t)(void);

/************************************************************
* @brief APP任务结构体
* @note  用于记录一个任务的执行函数、执行周期、上次运行时间和使能状态
************************************************************/
typedef struct
{
    AppTaskFunc_t task_func;     /* 任务函数 */
    uint32_t period_ms;          /* 任务执行周期 */
    uint32_t last_run_tick;      /* 上一次运行时间 */
    uint8_t  is_enable;          /* 任务使能标志 */

} AppTask_t;

/************************************************************
* @brief 任务函数定义
************************************************************/

/* LED1 心跳 1s 翻转 */
static void TaskLedHeartbeat(void)
{
    static uint8_t led_state = 0;
    led_state = !led_state;
    DrvLedCtrl(LED_ID_1, led_state);
}

/* 按键扫描 10ms */
static void TaskKeyScan(void)
{
    DrvKeyScan();
}

/* 按键事件处理 */
static void TaskKeyProcess(void)
{
    KeyValue_t key = DrvKeyReadValue();
    if (key.key_event == KEY_EVENT_NONE)
    {
        return;
    }

    printf("Key%d Event: ", key.key_id + 1);
    switch (key.key_event)
    {
        case KEY_EVENT_SHORT_PRESS:  printf("SHORT\r\n");  break;
        case KEY_EVENT_DOUBLE_PRESS: printf("DOUBLE\r\n"); break;
        case KEY_EVENT_LONG_PRESS:   printf("LONG\r\n");   break;
        default: break;
    }

    switch (key.key_id)
    {
        case KEY_ID_2:
            if (key.key_event == KEY_EVENT_SHORT_PRESS)
            {
                DrvLedCtrl(LED_ID_2, OPEN);
                printf("LED2 ON\r\n");
            }
            else if (key.key_event == KEY_EVENT_LONG_PRESS)
            {
                DrvLedCtrl(LED_ID_2, CLOSE);
                printf("LED2 OFF\r\n");
            }
            break;

        case KEY_ID_3:
            if (key.key_event == KEY_EVENT_SHORT_PRESS)
            {
                DrvLedCtrl(LED_ID_3, OPEN);
                printf("LED3 ON\r\n");
            }
            else if (key.key_event == KEY_EVENT_LONG_PRESS)
            {
                DrvLedCtrl(LED_ID_3, CLOSE);
                printf("LED3 OFF\r\n");
            }
            break;

        case KEY_ID_4:
            if (key.key_event == KEY_EVENT_DOUBLE_PRESS)
            {
                for (uint8_t i = 0; i < LED_ID_MAX; i++)
                {
                    DrvLedCtrl(i, CLOSE);
                }
                printf("All LEDs OFF\r\n");
            }
            break;

        default:
            break;
    }
}

/* 调试打印 */
static void TaskDebugPrint(void)
{
    printf("Tick: %u ms\r\n", (unsigned int)BspGetTick());
}

/************************************************************
* @brief 任务注册表
************************************************************/
static AppTask_t g_app_task[] =
{
    { TaskKeyScan,      10,    0, 1 },   /* 10ms 按键扫描 */
    { TaskKeyProcess,   10,    0, 1 },   /* 10ms 按键处理 */
    { TaskLedHeartbeat, 1000,  0, 1 },   /* 1s  LED心跳 */
    { TaskDebugPrint,   3000,  0, 1 },   /* 3s  调试打印 */
};

#define APP_TASK_NUM  (sizeof(g_app_task) / sizeof(g_app_task[0]))

/************************************************************
* @brief APP任务初始化
************************************************************/
void AppTaskInit(void)
{
    BspTickInit();
    DrvUARTInit(115200);
    DrvLedInit();
    DrvKeyInit();

    printf("=== System Started ===\r\n");
}

/************************************************************
* @brief APP任务运行函数，放在main的while(1)中
************************************************************/
void AppTaskRun(void)
{
    uint32_t current_tick = BspGetTick();

    for (uint8_t i = 0; i < APP_TASK_NUM; i++)
    {
        if (g_app_task[i].is_enable &&
            (current_tick - g_app_task[i].last_run_tick) >= g_app_task[i].period_ms)
        {
            g_app_task[i].last_run_tick = current_tick;
            g_app_task[i].task_func();
        }
    }
}
