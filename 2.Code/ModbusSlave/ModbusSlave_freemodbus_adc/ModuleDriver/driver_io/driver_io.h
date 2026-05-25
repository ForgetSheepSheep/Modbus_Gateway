#ifndef __DRIVER_IO
#define __DRIVER_IO

typedef enum
{
    IO_LED1 = 0,
    IO_LED2,
    IO_LED3,
    IO_BEEP1,
    IO_BEEP2,
    IO_RS485,
    IO_MAX
} IO_Index_t;

int GPIODriverInit(void);
int GPIODriverWrite(unsigned char io, unsigned char state);
int GPIODriverRead(unsigned char io);

#endif /* __DRIVER_IO */
