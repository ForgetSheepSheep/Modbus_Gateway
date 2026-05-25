#include "component_crc32/component_crc32.h"

uint32_t ComponentCRC32Update(uint32_t crc, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    while(len--)
    {
        crc ^= *buf++;

        for(i = 0; i < 8; i++)
        {
            if(crc & 1)
            {
                crc = (crc >> 1) ^ CRC32_POLY_VALUE;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint32_t ComponentCRC32Calc(uint8_t *buf, uint32_t len)
{
    uint32_t crc;

    crc = ComponentCRC32Update(CRC32_INIT_VALUE, buf, len);
    crc ^= CRC32_XOR_VALUE;

    return crc;
}
