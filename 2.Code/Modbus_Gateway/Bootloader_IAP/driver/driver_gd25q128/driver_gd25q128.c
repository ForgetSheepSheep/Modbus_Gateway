#include "driver_gd25q128/driver_gd25q128.h"
#include "driver_spi/driver_spi.h"

#include <string.h>

#define READ_CMD    0xFF

/************************************************************
* @brief GD25Q128驱动初始化
* @param 无
* @return 无
* @note  初始化底层SPI接口
************************************************************/
void DrvGD25Q128Init(void)
{
    DrvSPIInit();
}

/************************************************************
* @brief 读取GD25Q128的JEDEC ID
* @param 无
* @return 24位JEDEC ID，正常应为0xC84018
************************************************************/
uint32_t DrvGD25Q128ReadID(void)
{
    uint8_t ids[3] = {0};
    uint32_t id;

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_READ_ID);

    ids[0] = DrvSPIReadWriteByte(READ_CMD);
    ids[1] = DrvSPIReadWriteByte(READ_CMD);
    ids[2] = DrvSPIReadWriteByte(READ_CMD);

    SPI_FLASH_CS_HIGH();

    id = (((uint32_t)ids[0] << 16) | ((uint32_t)ids[1] << 8) | ids[2]);

    return id;
}

/************************************************************
* @brief GD25Q128写使能
* @param 无
* @return 无
* @note  页编程、扇区擦除、块擦除前都必须先调用
************************************************************/
void DrvGD25Q128WriteEnable(void)
{
    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_WRITE_ENABLE);

    SPI_FLASH_CS_HIGH();
}

/************************************************************
* @brief GD25Q128写失能
* @param 无
* @return 无
* @note  用于关闭写使能，防止误写入
************************************************************/
void DrvGD25Q128WriteDisable(void)
{
    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_WRITE_DISABLE);

    SPI_FLASH_CS_HIGH();
}

/************************************************************
* @brief 读取GD25Q128状态寄存器1
* @param 无
* @return 状态寄存器1的值
* @note  bit0=WIP忙标志，bit1=WEL写使能标志
************************************************************/
uint8_t DrvGD25Q128ReadStatus(void)
{
    uint8_t status = 0;

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_READ_STATUS);
    status = DrvSPIReadWriteByte(READ_CMD);

    SPI_FLASH_CS_HIGH();

    return status;
}

/************************************************************
* @brief 等待GD25Q128空闲
* @param 无
* @return 无
* @note  写入、擦除后必须等待WIP位清零
************************************************************/
void DrvGD25Q128WaitBusy(void)
{
    while(DrvGD25Q128ReadStatus() & GD25Q128_STATUS_BUSY)
    {
        /* 等待Flash内部写入或擦除完成 */
    }
}

/************************************************************
* @brief 擦除GD25Q128指定4KB扇区
* @param addr 扇区内任意地址，建议传入4KB对齐地址
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  擦除后该扇区内容变为0xFF
************************************************************/
uint8_t DrvGD25Q128SectorErase(uint32_t addr)
{
    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    DrvGD25Q128WriteEnable();

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_SECTOR_ERASE);

    DrvSPIReadWriteByte((uint8_t)(addr >> 16));
    DrvSPIReadWriteByte((uint8_t)(addr >> 8));
    DrvSPIReadWriteByte((uint8_t)addr);

    SPI_FLASH_CS_HIGH();

    DrvGD25Q128WaitBusy();

    return ESUCCESS;
}

/************************************************************
* @brief 擦除GD25Q128指定64KB块
* @param addr 块内任意地址，建议传入64KB对齐地址
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  64KB块擦除速度比4KB扇区擦除更适合大面积擦除
************************************************************/
uint8_t DrvGD25Q128BlockErase64K(uint32_t addr)
{
    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    DrvGD25Q128WriteEnable();

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_BLOCK_ERASE_64K);

    DrvSPIReadWriteByte((uint8_t)(addr >> 16));
    DrvSPIReadWriteByte((uint8_t)(addr >> 8));
    DrvSPIReadWriteByte((uint8_t)addr);

    SPI_FLASH_CS_HIGH();

    DrvGD25Q128WaitBusy();

    return ESUCCESS;
}

/************************************************************
* @brief GD25Q128单页写入
* @param addr 写入起始地址
* @param buf  写入数据缓冲区
* @param len  写入长度，最大256字节，不能跨页
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  页编程只能把1写成0，写入前需要先擦除
************************************************************/
uint8_t DrvGD25Q128PageWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t page_remain;

    if(buf == NULL)
    {
        return EFAIL;
    }

    if(len == 0)
    {
        return ESUCCESS;
    }

    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    if(len > (GD25Q128_SIZE - addr))
    {
        return EFAIL;
    }

    page_remain = GD25Q128_PAGE_SIZE - (addr % GD25Q128_PAGE_SIZE);

    if(len > page_remain)
    {
        return EFAIL;
    }

    DrvGD25Q128WriteEnable();

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_PAGE_PROGRAM);

    DrvSPIReadWriteByte((uint8_t)(addr >> 16));
    DrvSPIReadWriteByte((uint8_t)(addr >> 8));
    DrvSPIReadWriteByte((uint8_t)addr);

    for(i = 0; i < len; i++)
    {
        DrvSPIReadWriteByte(buf[i]);
    }

    SPI_FLASH_CS_HIGH();

    DrvGD25Q128WaitBusy();

    return ESUCCESS;
}

/************************************************************
* @brief GD25Q128连续写入数据
* @param addr 写入起始地址
* @param buf  写入数据缓冲区
* @param len  写入长度
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  内部会自动拆分为多次页写入，但不会自动擦除
************************************************************/
uint8_t DrvGD25Q128WriteBuf(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t write_addr;
    uint32_t write_len;
    uint32_t write_offset;
    uint16_t page_remain;

    if(buf == NULL)
    {
        return EFAIL;
    }

    if(len == 0)
    {
        return ESUCCESS;
    }

    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    if(len > (GD25Q128_SIZE - addr))
    {
        return EFAIL;
    }

    write_addr = addr;
    write_offset = 0;

    while(len > 0)
    {
        page_remain = GD25Q128_PAGE_SIZE - (write_addr % GD25Q128_PAGE_SIZE);

        if(len > page_remain)
        {
            write_len = page_remain;
        }
        else
        {
            write_len = len;
        }

        if(DrvGD25Q128PageWrite(write_addr, &buf[write_offset], (uint16_t)write_len) != ESUCCESS)
        {
            return EFAIL;
        }

        write_addr += write_len;
        write_offset += write_len;
        len -= write_len;
    }

    return ESUCCESS;
}

/************************************************************
* @brief 从GD25Q128连续读取数据
* @param addr 读取起始地址
* @param buf  读取数据存放缓冲区
* @param len  读取长度
* @return ESUCCESS表示成功，EFAIL表示失败
************************************************************/
uint8_t DrvGD25Q128ReadBuf(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if(buf == NULL)
    {
        return EFAIL;
    }

    if(len == 0)
    {
        return ESUCCESS;
    }

    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    if(len > (GD25Q128_SIZE - addr))
    {
        return EFAIL;
    }

    SPI_FLASH_CS_LOW();

    DrvSPIReadWriteByte(GD25Q128_CMD_READ_DATA);

    DrvSPIReadWriteByte((uint8_t)(addr >> 16));
    DrvSPIReadWriteByte((uint8_t)(addr >> 8));
    DrvSPIReadWriteByte((uint8_t)addr);

    for(i = 0; i < len; i++)
    {
        buf[i] = DrvSPIReadWriteByte(READ_CMD);
    }

    SPI_FLASH_CS_HIGH();

    return ESUCCESS;
}

/************************************************************
* @brief 擦除GD25Q128指定区域
* @param addr 擦除起始地址
* @param len  擦除长度
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  内部按4KB扇区擦除，会自动向扇区边界对齐
************************************************************/
uint8_t DrvGD25Q128EraseArea(uint32_t addr, uint32_t len)
{
    uint32_t erase_addr;
    uint32_t erase_end;

    if(len == 0)
    {
        return ESUCCESS;
    }

    if(addr >= GD25Q128_SIZE)
    {
        return EFAIL;
    }

    if(len > (GD25Q128_SIZE - addr))
    {
        return EFAIL;
    }

    erase_addr = addr - (addr % GD25Q128_SECTOR_SIZE);

    erase_end = addr + len;

    if(erase_end % GD25Q128_SECTOR_SIZE)
    {
        erase_end = erase_end + (GD25Q128_SECTOR_SIZE - (erase_end % GD25Q128_SECTOR_SIZE));
    }

    while(erase_addr < erase_end)
    {
        if(DrvGD25Q128SectorErase(erase_addr) != ESUCCESS)
        {
            return EFAIL;
        }

        erase_addr += GD25Q128_SECTOR_SIZE;
    }

    return ESUCCESS;
}

/************************************************************
* @brief GD25Q128基础测试函数
* @param 无
* @return ESUCCESS表示成功，EFAIL表示失败
* @note  测试流程：读ID -> 擦除 -> 写入 -> 读取 -> 对比
************************************************************/
uint8_t DrvGD25Q128Test(void)
{
    uint32_t id;
    uint8_t write_buf[32] = "GD25Q128 TEST";
    uint8_t read_buf[32] = {0};

    id = DrvGD25Q128ReadID();

    printf("FLASH ID: 0x%06X\r\n", id);

    if(id != GD25Q128_JEDEC_ID)
    {
        printf("FLASH ID ERROR\r\n");
        return EFAIL;
    }

    if(DrvGD25Q128EraseArea(0x000000, GD25Q128_SECTOR_SIZE) != ESUCCESS)
    {
        printf("FLASH ERASE ERROR\r\n");
        return EFAIL;
    }

    if(DrvGD25Q128WriteBuf(0x000000, write_buf, sizeof(write_buf)) != ESUCCESS)
    {
        printf("FLASH WRITE ERROR\r\n");
        return EFAIL;
    }

    if(DrvGD25Q128ReadBuf(0x000000, read_buf, sizeof(read_buf)) != ESUCCESS)
    {
        printf("FLASH READ ERROR\r\n");
        return EFAIL;
    }

    if(memcmp(write_buf, read_buf, sizeof(write_buf)) != 0)
    {
        printf("FLASH DATA CHECK ERROR\r\n");
        return EFAIL;
    }

    printf("FLASH TEST OK\r\n");
    printf("READ DATA: %s\r\n", read_buf);

    return ESUCCESS;
}
