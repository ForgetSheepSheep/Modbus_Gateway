#include "./driver_io/driver_io.h"
#include "stm32f0xx_hal.h"
#include "main.h"

#define ARRAY_SIZE(x)  ((sizeof(x)) / (sizeof((x)[0])))

typedef struct {
    GPIO_TypeDef *GPIOPort;
    uint16_t GPIOPin;
} GPIOInfo_t;

static GPIOInfo_t g_tGPIOInfo[] =
{
    { .GPIOPort = LED1_GPIO_Port,      .GPIOPin = LED1_Pin },
    { .GPIOPort = LED2_GPIO_Port,      .GPIOPin = LED2_Pin },
    { .GPIOPort = LED3_GPIO_Port,      .GPIOPin = LED3_Pin },
    { .GPIOPort = BEEP1_GPIO_Port,     .GPIOPin = BEEP1_Pin },
    { .GPIOPort = BEEP2_GPIO_Port,     .GPIOPin = BEEP2_Pin },
    { .GPIOPort = RS485_CTRL_GPIO_Port,.GPIOPin = RS485_CTRL_Pin },
};

int GPIODriverInit(void)
{
    return 0;
}

int GPIODriverWrite(unsigned char io, unsigned char state)
{
    GPIO_PinState pinstate;

    if (io >= ARRAY_SIZE(g_tGPIOInfo))
    {
        return -1;
    }
    if (state > 1)
    {
        return -2;
    }

    pinstate = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(g_tGPIOInfo[io].GPIOPort, g_tGPIOInfo[io].GPIOPin, pinstate);
    return 0;
}

int GPIODriverRead(unsigned char io)
{
    if (io >= ARRAY_SIZE(g_tGPIOInfo))
    {
        return -1;
    }

    return (int)HAL_GPIO_ReadPin(g_tGPIOInfo[io].GPIOPort, g_tGPIOInfo[io].GPIOPin);
}
