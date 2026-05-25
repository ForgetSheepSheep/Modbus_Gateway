#include "component_crc32/component_crc32.h"

/************************************************************
* @brief 更新CRC32校验值
* @param crc 当前CRC值
* @param buf 数据缓冲区
* @param len 数据长度
* @return 更新后的CRC32值
************************************************************/
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

/************************************************************
* @brief 计算一段数据的CRC32
* @param buf 数据缓冲区
* @param len 数据长度
* @return CRC32校验值（已异或最终值）
************************************************************/
uint32_t ComponentCRC32Calc(uint8_t *buf, uint32_t len)
{
    uint32_t crc;

    crc = ComponentCRC32Update(CRC32_INIT_VALUE, buf, len);
    crc ^= CRC32_XOR_VALUE;

    return crc;
}
