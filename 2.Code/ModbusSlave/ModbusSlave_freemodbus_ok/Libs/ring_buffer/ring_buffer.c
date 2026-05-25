#include "./ring_buffer/ring_buffer.h"


void ring_buffer_init(Pring_buffer_t pdst_buf, uint8_t *buf, uint16_t bufsize)
{
    if(pdst_buf == NULL || buf == NULL || bufsize <= 2) return;
    pdst_buf->buffer = buf;
    pdst_buf->bufsize = bufsize;
    pdst_buf->pr = 0;
    pdst_buf->pw = 0;
}


void ring_buffer_clear(Pring_buffer_t pdst_buf)
{
    if(pdst_buf == NULL) return;
    pdst_buf->pr = 0;
    pdst_buf->pw = 0;
}


void ring_buffer_write(uint8_t byte, Pring_buffer_t pdst_buf)
{
    if(pdst_buf == NULL || pdst_buf->buffer == NULL) return;
    uint16_t w_next = (pdst_buf->pw + 1) % pdst_buf->bufsize;
    if(w_next != pdst_buf->pr)  // �����ж�
    {
        pdst_buf->buffer[pdst_buf->pw] = byte;
        pdst_buf->pw = w_next;
    }
}


int ring_buffer_read(uint8_t *byte, Pring_buffer_t pdst_buf)
{
    if(pdst_buf == NULL || byte == NULL || pdst_buf->buffer == NULL)
        return EFAIL;

    if(pdst_buf->pr == pdst_buf->pw)  // �ӿ�
        return EFAIL;

    *byte = pdst_buf->buffer[pdst_buf->pr];
    pdst_buf->pr = (pdst_buf->pr + 1) % pdst_buf->bufsize;
    return ESUCCEES;
}
