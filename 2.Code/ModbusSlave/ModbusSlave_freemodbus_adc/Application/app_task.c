#include "app_task.h"
#include "./driver_systick/driver_systick.h"
#include <stdbool.h>
#include <stdint.h>
#include "app_modbusslave.h"

#define TASK_TABLE_NUM(x)   (sizeof(x) / sizeof((x)[0]))

typedef struct {
    bool is_enable;
    uint32_t period_ms;
    uint32_t reload_ms;
    void (*funtion)(void);
} TaskHandle_t;

static TaskHandle_t g_TaskTable[] =
{
    {.is_enable = false, .period_ms = 1, .reload_ms = 1, .funtion = ModbusTask},
};

static void AppTaskHook(void)
{
    for (uint8_t i = 0; i < TASK_TABLE_NUM(g_TaskTable); i++)
    {
        if (g_TaskTable[i].reload_ms == 0)
        {
            continue;
        }
        if (g_TaskTable[i].period_ms > 0)
        {
            g_TaskTable[i].period_ms--;
        }
        if (g_TaskTable[i].period_ms == 0)
        {
            g_TaskTable[i].is_enable = true;
            g_TaskTable[i].period_ms = g_TaskTable[i].reload_ms;
        }
    }
}

void AppTaskInit(void)
{
    SysTickRegisterCallback(AppTaskHook);
    ModbusAppInit();
}

void AppTaskLoop(void)
{
    for (uint8_t i = 0; i < TASK_TABLE_NUM(g_TaskTable); i++)
    {
        if (g_TaskTable[i].is_enable == true)
        {
            g_TaskTable[i].funtion();
            g_TaskTable[i].is_enable = false;
        }
    }
}
