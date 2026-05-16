#include "config.h"
#include "app_task.h"

#define APP_START_ADDR    0x08008000U

static void AppVectorTableInit(void)
{
    SCB->VTOR = APP_START_ADDR;
}

int main(void)
{
    AppVectorTableInit();
    __enable_irq();
    
    AppTaskInit();

    while (1)
    {
        AppTaskRun();
    }
}
