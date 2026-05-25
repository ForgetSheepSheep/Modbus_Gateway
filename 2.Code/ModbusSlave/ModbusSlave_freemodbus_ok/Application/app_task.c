#include "app_task.h"
#include "./driver_systick/driver_systick.h"
#include <stdbool.h>
#include "./driver_io/driver_io.h"
#include "./driver_key/driver_key.h"
#include "app_modbusslave.h"
#include "./driver_ah20/driver_ah20.h"

#define TASK_TABLE_NUM(x)   (sizeof(x) / sizeof((x)[0]))


static uint8_t led_state = 0;



static uint8_t relay_state = 0;

static uint16_t g_aht20_temp = 0;
static uint16_t g_aht20_humi = 0;

typedef enum {
    AHT20_IDLE,
    AHT20_WAIT_READY,
} AHT20_State_t;

static AHT20_State_t aht20_state = AHT20_IDLE;
static uint32_t aht20_tick = 0;

static void aht20_task(void)
{
    uint32_t now = SysTickGetTick();

    switch (aht20_state) {
    case AHT20_IDLE:
        if ((now - aht20_tick) >= 1000) {
            AHT20StartMeasure();
            aht20_tick = now;
            aht20_state = AHT20_WAIT_READY;
        }
        break;

    case AHT20_WAIT_READY:
        if ((now - aht20_tick) >= 80) {
            float temp, him;
            AHT20TempHimGetData(&temp, &him);
            g_aht20_temp = (uint16_t)(int16_t)(temp * 10);
            g_aht20_humi = (uint16_t)(him * 10);
            aht20_state = AHT20_IDLE;
        }
        break;
    }
}

uint16_t AppTaskGetAHT20Temp(void)
{
    return g_aht20_temp;
}

uint16_t AppTaskGetAHT20Humi(void)
{
    return g_aht20_humi;
}


#define KEY1_INDEX  0
#define KEY2_INDEX  1
#define KEY3_INDEX  2
#define SHORT_PRESS 0x10

static void test_key_scan(void)
{
    uint8_t key_val = KeyDriverGetValue(0);
    if(key_val == 0) return;

    uint8_t key_index = key_val & 0x0F;
    uint8_t key_event = key_val & 0xF0;

    if(key_event == SHORT_PRESS)
    {
        switch(key_index)
        {
            case KEY1_INDEX:
                led_state ^= 1;
                GPIODriverWrite(IO_LED1, led_state);
                break;
            case KEY2_INDEX:
                led_state ^= 1;
                GPIODriverWrite(IO_LED2, led_state);
                break;
            case KEY3_INDEX:
                relay_state ^= 1;
                GPIODriverWrite(IO_RELAY1, relay_state);
                break;
            default:
                break;
        }
    }
}
typedef struct {
    bool is_enable;
    uint32_t period_ms;
    uint32_t reload_ms;
    void (*funtion)(void); 
}TaskHandle_t;

static TaskHandle_t g_TaskTable[] =
{
    {.is_enable = false, .period_ms = 10,   .reload_ms = 10,   .funtion = test_key_scan},
    {.is_enable = false, .period_ms = 1,    .reload_ms = 1,    .funtion = ModbusTask},
    {.is_enable = false, .period_ms = 10,   .reload_ms = 10,   .funtion = aht20_task},
};

static void AppTaskHook(void)
{
    for(uint8_t i = 0; i <TASK_TABLE_NUM(g_TaskTable); i++)
    {
        if(g_TaskTable[i].reload_ms == 0)
        {
            continue;
        }
        if(g_TaskTable[i].period_ms > 0)
        {
            g_TaskTable[i].period_ms--;
        }
        if(g_TaskTable[i].period_ms == 0)
        {
            g_TaskTable[i].is_enable = true;
            g_TaskTable[i].period_ms = g_TaskTable[i].reload_ms;
        }
    }
}
void AppTaskInit(void)
{
    KeyDriverInit();
    SysTickRegisterCallback(AppTaskHook);

    /* 初始化 Modbus 从机 */
    ModbusAppInit();
}
void AppTaskLoop(void)
{
    for(uint8_t i = 0; i <TASK_TABLE_NUM(g_TaskTable); i++)
    {
        if(g_TaskTable[i].is_enable == true)
        {

            g_TaskTable[i].funtion();
            g_TaskTable[i].is_enable = false;
        }
    }
}

