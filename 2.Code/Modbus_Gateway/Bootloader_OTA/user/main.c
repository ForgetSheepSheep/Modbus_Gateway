#include "config.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_uart/driver_uart.h"
#include "./driver_delay/driver_delay.h"
#include "boot_main.h"


int main(void)
{
    BspTickInit();
    DrvUARTInit(115200);
    DrvDelayInit();

    printf("BOOT RUNNING\r\n");

    BootMainInit();

    while(1)
    {
        BootMainProcess();
    }
}
