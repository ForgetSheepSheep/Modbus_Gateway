#ifndef  __DRIVER_IO
#define  __DRIVER_IO

typedef enum
{
    IO_LED1 = 0,   // 对应 g_tGPIOInfo[0]
    IO_LED2,       // 对应 g_tGPIOInfo[1]
    IO_LED3,       // 对应 g_tGPIOInfo[2]
    IO_BEEP1,      // 对应 g_tGPIOInfo[3]
    IO_BEEP2,      // 对应 g_tGPIOInfo[4]
    IO_RELAY1,     // 对应 g_tGPIOInfo[5]
    IO_RELAY1B5,   // 对应 g_tGPIOInfo[6]
    IO_RS485,      // 对应 g_tGPIOInfo[7]
    IO_MAX         // 数量，用于循环边界
} IO_Index_t;

int GPIODriverInit(void);
int GPIODriverWrite(unsigned char io, unsigned char state);
int GPIODriverRead(unsigned char io);

#endif /* __DRIVER_IO */
