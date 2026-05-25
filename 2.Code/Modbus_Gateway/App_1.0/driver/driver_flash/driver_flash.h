#ifndef __DRIVER_FLASH_H
#define __DRIVER_FLASH_H
#include "config.h"
#include <stdint.h>

int32_t DrvFlashErase(uint32_t addr, uint32_t size);
int32_t DrvFlashWrite(uint32_t addr, uint8_t *buf, uint32_t len);

#endif /* __DRIVER_FLASH_H */
