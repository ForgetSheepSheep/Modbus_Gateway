#ifndef __BOOT_UPDATE_H
#define __BOOT_UPDATE_H

#include <stdint.h>

/* APP起始地址，根据Bootloader大小修改 */
#define BOOT_APP_START_ADDR       0x08004000U

/* APP最大空间，暂按256KB写，可根据Flash大小调整 */
#define BOOT_APP_MAX_SIZE         (256 * 1024U)

/* APP信息存放地址，放在最后一个页Flash */
#define BOOT_APP_INFO_ADDR        0x0807F800U

/* APP有效标志 */
#define BOOT_APP_MAGIC            0xA5A55A5AU

typedef struct
{
    uint32_t magic;        /* APP有效标志 */
    uint32_t app_addr;     /* APP起始地址 */
    uint32_t app_size;     /* APP文件大小 */
    uint32_t app_crc;      /* APP校验值，暂时先不使用 */

} BootAppInfo_t;

uint8_t BootUpdateByYmodem(void);
uint32_t BootUpdateGetFwSize(void);
uint8_t BootCopyExtFlashToApp(uint32_t fw_size);

#endif /* __BOOT_UPDATE_H */
