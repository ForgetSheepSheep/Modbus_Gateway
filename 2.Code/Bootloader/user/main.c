#include "config.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_uart/driver_uart.h"
#include "./driver_led/driver_led.h"
#include "boot_main.h"

/************************************************************
* @brief Bootloader主函数
* @param 无
* @return int
* @note  初始化基础外设，然后循环执行Bootloader状态机
************************************************************/
int main(void)
{
    BspTickInit();
    DrvUARTInit(115200);
    DrvLedInit();

    printf("BOOT RUNNING\r\n");

    BootMainInit();

    while(1)
    {
        BootMainProcess();
    }
}
