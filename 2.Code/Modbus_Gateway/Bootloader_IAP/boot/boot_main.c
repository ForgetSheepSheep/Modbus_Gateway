#include "boot_main.h"
#include "boot_jump.h"
#include "boot_update.h"
#include "./bsp_tick/bsp_tick.h"
#include "./driver_led/driver_led.h"
#include "./driver_uart/driver_uart.h"

#include <stdio.h>

#define BOOT_WAIT_CMD_TIMEOUT_MS    3000

typedef enum
{
    BOOT_STATE_INIT = 0,
    BOOT_STATE_CHECK_APP,
    BOOT_STATE_WAIT_CMD,
    BOOT_STATE_YMODEM_UPDATE,
    BOOT_STATE_COPY_FW,
    BOOT_STATE_JUMP_APP,
    BOOT_STATE_ERROR,

} BootState_e;

static BootState_e g_boot_state = BOOT_STATE_INIT;
static uint32_t g_fw_size = 0;

static uint8_t BootCheckUartCmd(void)
{
    uint8_t ch = 0;

    if(DrvUARTReadByte(&ch) == 0)
    {
        if(ch == 'U' || ch == 'u')
        {
            return 1;
        }
    }

    return 0;
}

void BootMainInit(void)
{
    g_boot_state = BOOT_STATE_INIT;
    g_fw_size = 0;
}

void BootMainProcess(void)
{
    static uint32_t wait_start = 0;

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
            if(BootCheckAppValid(APP_START_ADDR) == 1)
            {
                printf("APP VALID\r\n");
                g_boot_state = BOOT_STATE_WAIT_CMD;
            }
            else
            {
                printf("APP INVALID\r\n");
                printf("STAY IN BOOTLOADER\r\n");
                g_boot_state = BOOT_STATE_YMODEM_UPDATE;
            }
            break;
        }

        case BOOT_STATE_WAIT_CMD:
        {
            if(wait_start == 0)
            {
                wait_start = BspGetTick();
                printf("Press U to update (3s timeout)\r\n");
            }

            if(BootCheckUartCmd())
            {
                printf("Enter YMODEM mode\r\n");
                wait_start = 0;
                g_boot_state = BOOT_STATE_YMODEM_UPDATE;
                break;
            }

            if((BspGetTick() - wait_start) >= BOOT_WAIT_CMD_TIMEOUT_MS)
            {
                printf("Timeout, jump to APP\r\n");
                wait_start = 0;
                g_boot_state = BOOT_STATE_JUMP_APP;
            }
            break;
        }

        case BOOT_STATE_YMODEM_UPDATE:
        {
            if(BootUpdateByYmodem() == ESUCCESS)
            {
                g_boot_state = BOOT_STATE_COPY_FW;
            }
            else
            {
                printf("YMODEM UPDATE FAIL\r\n");
                g_boot_state = BOOT_STATE_ERROR;
            }
            break;
        }

        case BOOT_STATE_COPY_FW:
        {
            g_fw_size = BootUpdateGetFwSize();

            if(BootCopyExtFlashToApp(g_fw_size) == ESUCCESS)
            {
                printf("JUMP TO NEW APP\r\n");
                g_boot_state = BOOT_STATE_JUMP_APP;
            }
            else
            {
                printf("COPY FW FAIL\r\n");
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
                DrvLedCtrl(LED_ID_1, led_state ? OPEN : CLOSE);
            }

            if(BootCheckUartCmd())
            {
                printf("Retry YMODEM update\r\n");
                g_boot_state = BOOT_STATE_YMODEM_UPDATE;
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
