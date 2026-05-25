#include "component_ymodem.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/************************************************************
* @brief ����YMODEMʹ�õ�CRC16У��ֵ
* @param pdata ���ݻ�����ָ��
* @param len   ���ݳ���
* @return CRC16У��ֵ
************************************************************/
static uint16_t ComponentYmodemCalcCrc16(uint8_t *pdata, uint32_t len)
{
    uint16_t crc = 0;
    uint8_t j;

    while(len--)
    {
        crc ^= (uint16_t)(*pdata++) << 8;

        for(j = 0; j < 8; j++)
        {
            if(crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

/************************************************************
* @brief ����YMODEM��һ���е��ļ������ļ���С
* @param handle YMODEM������
* @param pdata  ��һ��������ָ��
* @return YMODEM_RET_OK��ʾ�����ɹ�
************************************************************/
static YmodemRet_e ComponentYmodemParseFileInfo(YmodemHandle_t *handle, uint8_t *pdata)
{
    uint32_t i = 0;
    char *psize = NULL;

    if(handle == NULL || pdata == NULL)
    {
        return YMODEM_RET_ERROR;
    }

    memset(&handle->file_info, 0, sizeof(YmodemFileInfo_t));

    /* ��һ����һ���ֽھ���0��˵�����ǽ����հ� */
    if(pdata[0] == 0)
    {
        return YMODEM_RET_OK;
    }

    /* �����ļ��� */
    while((pdata[i] != 0) && (i < sizeof(handle->file_info.file_name) - 1))
    {
        handle->file_info.file_name[i] = pdata[i];
        i++;
    }

    handle->file_info.file_name[i] = '\0';

    /* �ļ��������һ��0���������ļ���С�ַ��� */
    psize = (char *)&pdata[i + 1];

    handle->file_info.file_size = (uint32_t)strtoul(psize, NULL, 10);
    handle->file_info.recv_size = 0;

    return YMODEM_RET_OK;
}

/************************************************************
* @brief ����һ��YMODEM���ݰ�
* @param handle      YMODEM������
* @param packet_buf  ���ݰ�������
* @param packet_size ���������������������С��128��1024
* @return YMODEM���ս��
************************************************************/
static YmodemRet_e ComponentYmodemRecvPacket(YmodemHandle_t *handle,
                                             uint8_t *packet_buf,
                                             uint32_t *packet_size)
{
    uint8_t ch = 0;
    uint32_t total_size = 0;
    uint32_t i = 0;
    uint16_t recv_crc = 0;
    uint16_t calc_crc = 0;

    if(handle == NULL || packet_buf == NULL || packet_size == NULL)
    {
        return YMODEM_RET_ERROR;
    }

    if(handle->recv_byte(&ch, YMODEM_RX_TIMEOUT_MS) != 0)
    {
        return YMODEM_RET_TIMEOUT;
    }

    packet_buf[0] = ch;

    if(ch == YMODEM_SOH)
    {
        *packet_size = YMODEM_PACKET_128_SIZE;
        total_size = YMODEM_PACKET_128_SIZE + YMODEM_PACKET_OVERHEAD;
    }
    else if(ch == YMODEM_STX)
    {
        *packet_size = YMODEM_PACKET_1K_SIZE;
        total_size = YMODEM_PACKET_1K_SIZE + YMODEM_PACKET_OVERHEAD;
    }
    else if(ch == YMODEM_EOT)
    {
        return YMODEM_RET_OK;
    }
    else if(ch == YMODEM_CAN)
    {
        return YMODEM_RET_CANCEL;
    }
    else
    {
        return YMODEM_RET_PACKET_ERROR;
    }

    /* �Ѿ����˵�1���ֽڣ��ӵ�2���ֽڼ������� */
    for(i = 1; i < total_size; i++)
    {
        if(handle->recv_byte(&packet_buf[i], YMODEM_RX_TIMEOUT_MS) != 0)
        {
            return YMODEM_RET_TIMEOUT;
        }
    }

    /* �����źͰ��ŷ��� */
    if((packet_buf[1] + packet_buf[2]) != 0xFF)
    {
        return YMODEM_RET_PACKET_ERROR;
    }

    /* ��������ֽ���CRC16�����ֽ���ǰ */
    recv_crc = ((uint16_t)packet_buf[total_size - 2] << 8) | packet_buf[total_size - 1];

    /* ��������packet_buf[3]��ʼ */
    calc_crc = ComponentYmodemCalcCrc16(&packet_buf[3], *packet_size);

    if(recv_crc != calc_crc)
    {
        return YMODEM_RET_CRC_ERROR;
    }

    return YMODEM_RET_OK;
}

/************************************************************
* @brief ��ʼ��YMODEM���
* @param handle     YMODEM������
* @param recv_byte  ����1�ֽڻص�����
* @param send_byte  ����1�ֽڻص�����
* @param write_data д���ݻص�����
* @return ��
************************************************************/
void ComponentYmodemInit(YmodemHandle_t *handle,
                         YmodemRecvByteCallback_t recv_byte,
                         YmodemSendByteCallback_t send_byte,
                         YmodemWriteCallback_t write_data)
{
    if(handle == NULL)
    {
        return;
    }

    memset(handle, 0, sizeof(YmodemHandle_t));

    handle->recv_byte  = recv_byte;
    handle->send_byte  = send_byte;
    handle->write_data = write_data;
}

/************************************************************
* @brief YMODEM����������
* @param handle YMODEM������
* @return YMODEM���ս��
* @note  ��һ�������ļ������ļ���С��������ͨ��write_data�ص�д��Flash
************************************************************/
YmodemRet_e ComponentYmodemReceive(YmodemHandle_t *handle)
{
    static uint8_t packet_buf[YMODEM_PACKET_MAX_SIZE];

    YmodemRet_e ret;
    uint32_t packet_size = 0;
    uint8_t packet_num = 0;
    uint8_t retry = 0;
    uint32_t write_len = 0;

    if(handle == NULL)
    {
        return YMODEM_RET_ERROR;
    }

    if(handle->recv_byte == NULL || handle->send_byte == NULL || handle->write_data == NULL)
    {
        return YMODEM_RET_ERROR;
    }

    packet_num = 0;
    retry = 0;

    while(1)
    {
        /* ����'C'��֪ͨ��λ��ʹ��CRCģʽ����YMODEM */
        handle->send_byte(YMODEM_CRC);

        ret = ComponentYmodemRecvPacket(handle, packet_buf, &packet_size);

        if(ret == YMODEM_RET_TIMEOUT)
        {
            retry++;

            if(retry >= YMODEM_RETRY_MAX)
            {
                return YMODEM_RET_TIMEOUT;
            }

            continue;
        }

        if(ret != YMODEM_RET_OK)
        {
            handle->send_byte(YMODEM_NAK);
            return ret;
        }

        /* �յ���һ���������ļ���Ϣ */
        if(packet_buf[0] == YMODEM_SOH || packet_buf[0] == YMODEM_STX)
        {
            if(packet_buf[1] != 0x00)
            {
                handle->send_byte(YMODEM_NAK);
                return YMODEM_RET_PACKET_ERROR;
            }

            ret = ComponentYmodemParseFileInfo(handle, &packet_buf[3]);

            if(ret != YMODEM_RET_OK)
            {
                handle->send_byte(YMODEM_NAK);
                return ret;
            }

            handle->send_byte(YMODEM_ACK);
            handle->send_byte(YMODEM_CRC);

            break;
        }
    }

    packet_num = 1;

    while(1)
    {
        ret = ComponentYmodemRecvPacket(handle, packet_buf, &packet_size);

        if(ret == YMODEM_RET_TIMEOUT)
        {
            retry++;

            if(retry >= YMODEM_RETRY_MAX)
            {
                return YMODEM_RET_TIMEOUT;
            }

            handle->send_byte(YMODEM_CRC);
            continue;
        }

        if(ret == YMODEM_RET_CANCEL)
        {
            return YMODEM_RET_CANCEL;
        }

        /* EOT: 文件数据传输结束 */
        if(packet_buf[0] == YMODEM_EOT)
        {
            handle->send_byte(YMODEM_ACK);

            printf("[YMODEM] EOT\r\n");
            printf("[YMODEM] receive finish\r\n");
            printf("[YMODEM] receive size = %lu\r\n", handle->file_info.recv_size);

            return YMODEM_RET_OK;
        }

        if(ret != YMODEM_RET_OK)
        {
            handle->send_byte(YMODEM_NAK);
            return ret;
        }

        /* �жϰ����Ƿ���ȷ */
        if(packet_buf[1] != packet_num)
        {
            handle->send_byte(YMODEM_NAK);
            return YMODEM_RET_PACKET_ERROR;
        }

        /* ���һ������ȫ��д�룬Ҫ����ʵ�ļ���С�ü� */
        if((handle->file_info.recv_size + packet_size) > handle->file_info.file_size)
        {
            write_len = handle->file_info.file_size - handle->file_info.recv_size;
        }
        else
        {
            write_len = packet_size;
        }

        if(write_len > 0)
        {
            ret = handle->write_data(handle->file_info.recv_size, &packet_buf[3], write_len);

            if(ret != YMODEM_RET_OK)
            {
                handle->send_byte(YMODEM_NAK);
                return ret;
            }

            handle->file_info.recv_size += write_len;
        }

        handle->send_byte(YMODEM_ACK);

        packet_num++;
        retry = 0;
    }

    return YMODEM_RET_OK;
}
