#ifndef  __BOOT_JUMP_H
#define  __BOOT_JUMP_H
#include "config.h"



#define APP_START_ADDR    0x08004000U

uint8_t BootCheckAppValid(uint32_t app_addr);
void BootJumpToApp(uint32_t app_addr);

#endif /* __BOOT_JUMP_H */
