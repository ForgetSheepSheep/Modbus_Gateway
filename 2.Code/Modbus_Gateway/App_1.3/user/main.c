#include "config.h"
#include "app_task.h"

#define APP_START_ADDR    0x08004000U



int main(void)
{
    SCB->VTOR = 0x08004000U;

    AppTaskInit();
    printf("[APP 1.3] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
    while (1)
    {
        AppTaskRun();
    }
}
