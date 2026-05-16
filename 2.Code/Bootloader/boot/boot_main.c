#include "boot_main.h"
#include "boot_jump.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_led/driver_led.h"


/************************************************************
* @brief Bootloader状态枚举
* @note  用于控制Bootloader当前运行到哪个阶段
************************************************************/
typedef enum
{
    BOOT_STATE_INIT = 0,       /* 初始化状态 */
    BOOT_STATE_CHECK_APP,      /* 检查APP是否合法 */
    BOOT_STATE_JUMP_APP,       /* 跳转APP */
    BOOT_STATE_ERROR,          /* 错误状态，停留在Bootloader */

} BootState_e;

static BootState_e g_boot_state = BOOT_STATE_INIT;

/************************************************************
* @brief Bootloader主流程初始化
* @param 无
* @return 无
************************************************************/
void BootMainInit(void)
{
    g_boot_state = BOOT_STATE_INIT;
}

/************************************************************
* @brief Bootloader主状态机处理函数
* @param 无
* @return 无
* @note  当前版本只完成最小Bootloader跳转APP流程
************************************************************/
void BootMainProcess(void)
{
    switch(g_boot_state)
    {
        case BOOT_STATE_INIT:
        {
            printf("BOOT STATE INIT\r\n");
            g_boot_state = BOOT_STATE_CHECK_APP;
            break;
        }

        case BOOT_STATE_CHECK_APP:
        {
            if(BootCheckAppValid(APP_START_ADDR))
            {
                printf("APP VALID\r\n");
                g_boot_state = BOOT_STATE_JUMP_APP;
            }
            else
            {
                printf("APP INVALID\r\n");
                printf("STAY IN BOOTLOADER\r\n");
                g_boot_state = BOOT_STATE_ERROR;
            }
            break;
        }

        case BOOT_STATE_JUMP_APP:
        {
            printf("JUMP TO APP\r\n");



            BootJumpToApp(APP_START_ADDR);
            break;
        }

        case BOOT_STATE_ERROR:
        {
            static uint32_t last_time = 0;
            static uint8_t led_state = 0;

            if(BspGetTick() - last_time >= 500)
            {
                last_time = BspGetTick();

                led_state = !led_state;

                if(led_state)
                {
                    DrvLedCtrl(LED_ID_1, OPEN);
                }
                else
                {
                    DrvLedCtrl(LED_ID_1, CLOSE);
                }
            }

            break;
        }

        default:
        {
            g_boot_state = BOOT_STATE_ERROR;
            break;
        }
    }
}
