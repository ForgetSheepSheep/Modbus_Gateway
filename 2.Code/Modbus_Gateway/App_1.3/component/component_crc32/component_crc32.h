#ifndef __COMPONENT_CRC32_H
#define __COMPONENT_CRC32_H

#include <stdint.h>

#define CRC32_INIT_VALUE      0xFFFFFFFFUL
#define CRC32_XOR_VALUE       0xFFFFFFFFUL
#define CRC32_POLY_VALUE      0xEDB88320UL

uint32_t ComponentCRC32Update(uint32_t crc, uint8_t *buf, uint32_t len);
uint32_t ComponentCRC32Calc(uint8_t *buf, uint32_t len);

#endif /* __COMPONENT_CRC32_H */
