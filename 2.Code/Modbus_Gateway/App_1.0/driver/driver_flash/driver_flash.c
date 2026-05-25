#include "./driver_flash/driver_flash.h"

#include "gd32f30x.h"

#define FLASH_PAGE_SIZE     2048UL

/************************************************************
* @brief 擦除内部Flash指定区域
* @param addr 起始地址（必须页对齐）
* @param size 擦除大小（字节）
* @return 0表示成功，非0表示失败
************************************************************/
int32_t DrvFlashErase(uint32_t addr, uint32_t size)
{
    uint32_t erase_addr;
    uint32_t end_addr;
    fmc_state_enum fmc_state;

    if(size == 0)
    {
        return 0;
    }

    end_addr = addr + size;

    /* 向上对齐到页边界 */
    end_addr = (end_addr + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);

    fmc_unlock();

    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);

    for(erase_addr = addr; erase_addr < end_addr; erase_addr += FLASH_PAGE_SIZE)
    {
        fmc_state = fmc_page_erase(erase_addr);

        if(fmc_state != FMC_READY)
        {
            fmc_lock();
            return -1;
        }
    }

    fmc_lock();

    return 0;
}

/************************************************************
* @brief 向内部Flash写入数据
* @param addr 写入地址（必须4字节对齐）
* @param buf  数据缓冲区
* @param len  数据长度（字节）
* @return 0表示成功，非0表示失败
************************************************************/
int32_t DrvFlashWrite(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint32_t write_data;
    uint32_t remain;
    fmc_state_enum fmc_state;

    if(buf == NULL || len == 0)
    {
        return 0;
    }

    fmc_unlock();

    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);

    i = 0;

    while(i < len)
    {
        remain = len - i;

        write_data = 0xFFFFFFFFUL;

        if(remain >= 4)
        {
            write_data = ((uint32_t)buf[i]) |
                         ((uint32_t)buf[i + 1] << 8) |
                         ((uint32_t)buf[i + 2] << 16) |
                         ((uint32_t)buf[i + 3] << 24);
        }
        else
        {
            if(remain >= 1)
            {
                write_data &= 0xFFFFFF00UL;
                write_data |= (uint32_t)buf[i];
            }
            if(remain >= 2)
            {
                write_data &= 0xFFFF00FFUL;
                write_data |= (uint32_t)buf[i + 1] << 8;
            }
            if(remain >= 3)
            {
                write_data &= 0xFF00FFFFUL;
                write_data |= (uint32_t)buf[i + 2] << 16;
            }
        }

        fmc_state = fmc_word_program(addr, write_data);

        if(fmc_state != FMC_READY)
        {
            fmc_lock();
            return -1;
        }

        if(*(volatile uint32_t *)addr != write_data)
        {
            fmc_lock();
            return -1;
        }

        addr += 4;

        if(remain >= 4)
        {
            i += 4;
        }
        else
        {
            i += remain;
        }
    }

    fmc_lock();

    return 0;
}
