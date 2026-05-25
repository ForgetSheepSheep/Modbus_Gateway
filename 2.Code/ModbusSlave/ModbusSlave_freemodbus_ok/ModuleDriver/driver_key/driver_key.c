#include "driver_key.h"
#include "stm32f0xx_hal.h"
#include "main.h"
#include "./driver_systick/driver_systick.h"
#include "./ring_buffer/ring_buffer.h"

/*
 * 错误：static Pring_buffer_t g_ptkeyRingBuffer;  // 指针未指向有效内存，init收到NULL直接返回
 * 正确：先定义结构体变量分配内存，再用指针指向它
 */
static ring_buffer_t g_tRingBufData;                              // 实际结构体，分配内存
static Pring_buffer_t g_ptkeyRingBuffer = &g_tRingBufData;        // 指针指向结构体
static uint8_t keybuff[16];



typedef struct GPIOInof
{
	GPIO_TypeDef *GPIOPort;
	uint16_t GPIOPin;
}GPIOInfo_t, *PGPIOInfo_t;

static GPIOInfo_t g_tGPIOInfo[] =
{
    { .GPIOPort = KEY1_GPIO_Port,     .GPIOPin = KEY1_Pin },
    { .GPIOPort = KEY2_GPIO_Port,     .GPIOPin = KEY2_Pin },
    { .GPIOPort = KEY3_GPIO_Port,     .GPIOPin = KEY3_Pin },
};

#define KEY_NUM_MAX  (sizeof(g_tGPIOInfo) / sizeof((g_tGPIOInfo)[0]))
#define CONFIRM_TIME 10 /* 10ms */

typedef enum 
{
    IDLE_STATE = 0,
    CONFIRM_STATE,
    SHORT_STATE,
}KEY_STATE;

typedef struct{
    KEY_STATE keystate;
    uint32_t prsystick; // ��������ʱ���
}KeyInfo_t;

static KeyInfo_t g_keyinfo[KEY_NUM_MAX];

int KeyDriverInit(void)
{
    ring_buffer_init(g_ptkeyRingBuffer, keybuff, sizeof(keybuff));
    return 0;
}

static void keyscan(uint8_t index)
{
    uint8_t value = 0;
    uint8_t ispress;
    uint32_t tick = SysTickGetTick();

    ispress = HAL_GPIO_ReadPin(g_tGPIOInfo[index].GPIOPort, g_tGPIOInfo[index].GPIOPin) ? 0 : 1;

    switch(g_keyinfo[index].keystate)
    {
        case IDLE_STATE:
            if(ispress)
            {
                g_keyinfo[index].keystate = CONFIRM_STATE;
                g_keyinfo[index].prsystick = tick;
            }
            break;
        case CONFIRM_STATE:
            if(ispress)
            {
                if(tick - g_keyinfo[index].prsystick >= CONFIRM_TIME)
                {
                    g_keyinfo[index].keystate = SHORT_STATE;
                }
            }
            else
            {
                g_keyinfo[index].keystate = IDLE_STATE;
            }
            break;
        case SHORT_STATE:
            if(!ispress)
            {
                value = index | SHORT_VALUE;
                ring_buffer_write(value, g_ptkeyRingBuffer);
                g_keyinfo[index].keystate = IDLE_STATE;
            }
            break;
        default:
            g_keyinfo[index].keystate = IDLE_STATE;
            break;
    }
}

uint8_t KeyDriverGetValue(uint8_t index)
{
    uint8_t value = 0;
    for(uint8_t i = 0; i < KEY_NUM_MAX; i++)
    {
        keyscan(i);
    }
    ring_buffer_read(&value, g_ptkeyRingBuffer);
    return value;
}
