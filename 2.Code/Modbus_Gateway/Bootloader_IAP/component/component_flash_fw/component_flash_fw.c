#include "component_flash_fw/component_flash_fw.h"
#include "component_crc32/component_crc32.h"
#include "driver_gd25q128/driver_gd25q128.h"

/************************************************************
* @brief ���̼����������ʷ�Χ�Ƿ�Ϸ�
* @param offset �̼���������ƫ�Ƶ�ַ
* @param len    ���ʳ���
* @return ESUCCESS��ʾ�Ϸ���EFAIL��ʾ�Ƿ�
************************************************************/
static uint8_t FlashFwCheckDownloadRange(uint32_t offset, uint32_t len)
{
    if(len == 0)
    {
        return ESUCCESS;
    }

    if(offset >= FLASH_FW_AREA_SIZE)
    {
        return EFAIL;
    }

    if(len > (FLASH_FW_AREA_SIZE - offset))
    {
        return EFAIL;
    }

    return ESUCCESS;
}
/************************************************************
* @brief �ⲿFlash�̼�����ģ���ʼ��
* @param ��
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
************************************************************/
uint8_t FlashFwInit(void)
{
    uint32_t id;

    DrvGD25Q128Init();

    id = DrvGD25Q128ReadID();

    printf("FLASH ID: 0x%06X\r\n", id);

    if(id != GD25Q128_JEDEC_ID)
    {
        return EFAIL;
    }

    return ESUCCESS;
}
/************************************************************
* @brief �����ⲿFlash�¹̼�������
* @param ��
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
************************************************************/
uint8_t FlashFwEraseDownloadArea(void)
{
    return DrvGD25Q128EraseArea(FLASH_FW_DOWNLOAD_ADDR, FLASH_FW_AREA_SIZE);
}

/************************************************************
* @brief ���¹̼�������д������
* @param offset �̼���������ƫ�Ƶ�ַ
* @param buf    ���ݻ�����
* @param len    д�볤��
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
* @note  д��ǰ��Ҫ�Ȳ�����Ӧ����
************************************************************/
uint8_t FlashFwWriteDownload(uint32_t offset, uint8_t *buf, uint32_t len)
{
    if(buf == NULL)
    {
        return EFAIL;
    }

    if(FlashFwCheckDownloadRange(offset, len) != ESUCCESS)
    {
        return EFAIL;
    }

    return DrvGD25Q128WriteBuf(FLASH_FW_DOWNLOAD_ADDR + offset, buf, len);
}
/************************************************************
* @brief ���¹̼���������ȡ����
* @param offset �̼���������ƫ�Ƶ�ַ
* @param buf    ��ȡ���ݴ�Ż�����
* @param len    ��ȡ����
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
************************************************************/
uint8_t FlashFwReadDownload(uint32_t offset, uint8_t *buf, uint32_t len)
{
    if(buf == NULL)
    {
        return EFAIL;
    }

    if(FlashFwCheckDownloadRange(offset, len) != ESUCCESS)
    {
        return EFAIL;
    }

    return DrvGD25Q128ReadBuf(FLASH_FW_DOWNLOAD_ADDR + offset, buf, len);
}
#define FLASH_FW_CRC_BUF_SIZE      256U
/************************************************************
* @brief �����ⲿFlash�¹̼���������CRC32
* @param fw_size �̼���С����λ�ֽ�
* @return ����õ���CRC32ֵ����������ʱ����0
* @note  ���ⲿFlash�������ֿ��ȡ���ݣ���������CRC32
************************************************************/
uint32_t FlashFwCalcDownloadCRC32(uint32_t fw_size)
{
    uint8_t read_buf[FLASH_FW_CRC_BUF_SIZE];
    uint32_t crc;
    uint32_t offset;
    uint32_t read_len;

    if(fw_size == 0)
    {
        return 0;
    }

    if(fw_size > FLASH_FW_AREA_SIZE)
    {
        return 0;
    }

    crc = CRC32_INIT_VALUE;
    offset = 0;

    while(offset < fw_size)
    {
        if((fw_size - offset) > FLASH_FW_CRC_BUF_SIZE)
        {
            read_len = FLASH_FW_CRC_BUF_SIZE;
        }
        else
        {
            read_len = fw_size - offset;
        }

        if(FlashFwReadDownload(offset, read_buf, read_len) != ESUCCESS)
        {
            return 0;
        }

        crc = ComponentCRC32Update(crc, read_buf, read_len);

        offset += read_len;
    }

    crc ^= CRC32_XOR_VALUE;

    return crc;
}
/************************************************************
* @brief д��̼���Ϣͷ
* @param info �̼���Ϣ�ṹ��ָ��
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
* @note  д��ǰ�������Ϣͷ��������
************************************************************/
uint8_t FlashFwWriteInfo(FlashFwInfo_t *info)
{
    if(info == NULL)
    {
        return EFAIL;
    }

    info->magic = FLASH_FW_INFO_MAGIC;

    if(DrvGD25Q128EraseArea(FLASH_FW_INFO_ADDR, sizeof(FlashFwInfo_t)) != ESUCCESS)
    {
        return EFAIL;
    }

    if(DrvGD25Q128WriteBuf(FLASH_FW_INFO_ADDR, (uint8_t *)info, sizeof(FlashFwInfo_t)) != ESUCCESS)
    {
        return EFAIL;
    }

    return ESUCCESS;
}

/************************************************************
* @brief ��ȡ�̼���Ϣͷ
* @param info �̼���Ϣ�ṹ��ָ��
* @return ESUCCESS��ʾ�ɹ���EFAIL��ʾʧ��
************************************************************/
uint8_t FlashFwReadInfo(FlashFwInfo_t *info)
{
    if(info == NULL)
    {
        return EFAIL;
    }

    return DrvGD25Q128ReadBuf(FLASH_FW_INFO_ADDR, (uint8_t *)info, sizeof(FlashFwInfo_t));
}

/************************************************************
* @brief ���̼���Ϣͷ�Ƿ�Ϸ�
* @param info �̼���Ϣ�ṹ��ָ��
* @return ESUCCESS��ʾ��Ч��EFAIL��ʾ��Ч
************************************************************/
uint8_t FlashFwCheckInfo(FlashFwInfo_t *info)
{
    if(info == NULL)
    {
        return EFAIL;
    }

    if(info->magic != FLASH_FW_INFO_MAGIC)
    {
        return EFAIL;
    }

    if(info->fw_size == 0)
    {
        return EFAIL;
    }

    if(info->fw_size > FLASH_FW_AREA_SIZE)
    {
        return EFAIL;
    }

    if(info->fw_addr != FLASH_FW_DOWNLOAD_ADDR)
    {
        return EFAIL;
    }

    return ESUCCESS;
}
