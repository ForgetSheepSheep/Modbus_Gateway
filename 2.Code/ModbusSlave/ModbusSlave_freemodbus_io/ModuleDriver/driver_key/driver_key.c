#include "driver_key.h"
#include "stm32f0xx_hal.h"
#include "main.h"

#define ARRAY_SIZE(x)  ((sizeof(x)) / (sizeof((x)[0])))

typedef struct {
    GPIO_TypeDef *GPIOPort;
    uint16_t GPIOPin;
} GPIOInfo_t;

static GPIOInfo_t g_tGPIOInfo[] =
{
    { .GPIOPort = KEY1_GPIO_Port, .GPIOPin = KEY1_Pin },
    { .GPIOPort = KEY2_GPIO_Port, .GPIOPin = KEY2_Pin },
    { .GPIOPort = KEY3_GPIO_Port, .GPIOPin = KEY3_Pin },
};

int KeyDriverInit(void)
{
    return 0;
}

uint8_t KeyDriverReadState(uint8_t index)
{
    if (index >= ARRAY_SIZE(g_tGPIOInfo))
    {
        return 0;
    }

    return (HAL_GPIO_ReadPin(g_tGPIOInfo[index].GPIOPort, g_tGPIOInfo[index].GPIOPin) == GPIO_PIN_RESET) ? 1U : 0U;
}

uint8_t KeyDriverGetValue(uint8_t index)
{
    (void)index;
    return 0;
}
