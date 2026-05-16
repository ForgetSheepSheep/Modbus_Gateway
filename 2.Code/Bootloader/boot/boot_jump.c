#include "boot_jump.h"

/************************************************************
* @brief 检查APP程序是否合法
* @param app_addr APP程序起始地址
* @return 1表示APP合法，0表示APP不合法
* @note  主要检查APP栈顶地址和复位入口地址是否在合理范围内
************************************************************/
uint8_t BootCheckAppValid(uint32_t app_addr)
{
    uint32_t app_stack;
    uint32_t app_entry;

    app_stack = *(volatile uint32_t *)app_addr;
    app_entry = *(volatile uint32_t *)(app_addr + 4U);

    printf("APP STACK = 0x%08X\r\n", app_stack);
    printf("APP ENTRY = 0x%08X\r\n", app_entry);

    if((app_stack & 0x2FFE0000U) != 0x20000000U)
    {
        return 0;
    }

    if((app_entry & 0xFF000000U) != 0x08000000U)
    {
        return 0;
    }

    return 1;
}

/************************************************************
* @brief 跳转到APP程序
* @param app_addr APP程序起始地址
* @return 无
* @note  跳转前关闭中断、关闭SysTick、设置MSP，然后跳转到APP复位入口
************************************************************/
void BootJumpToApp(uint32_t app_addr)
{
    uint32_t app_stack;
    uint32_t app_entry;
    void (*app_func)(void);

    app_stack = *(volatile uint32_t *)app_addr;
    app_entry = *(volatile uint32_t *)(app_addr + 4U);
    /* 用全新的姿态进入APP的准备 */
    __disable_irq();
    
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    SCB->VTOR = app_addr;
    __set_MSP(app_stack);
    
    app_func = (void (*)(void))app_entry;
    app_func();
}
