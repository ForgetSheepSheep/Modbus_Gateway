#ifndef  __RING_BUFFER_H
#define  __RING_BUFFER_H
#include "config.h"


typedef struct
{
    uint8_t *buffer;
    volatile uint16_t bufsize;
    volatile uint16_t pw;
    volatile uint16_t pr;
    
}ring_buffer_t, *Pring_buffer_t;

void ring_buffer_init(Pring_buffer_t pdst_buf, uint8_t *buf, uint16_t bufsize);
void ring_buffer_clear(Pring_buffer_t pdst_buf);
void ring_buffer_write(uint8_t byte, Pring_buffer_t pdst_buf);
int ring_buffer_read(uint8_t *byte, Pring_buffer_t pdst_buf);

#endif /* __RING_BUFFER_H */
