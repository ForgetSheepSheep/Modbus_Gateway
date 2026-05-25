#ifndef  __DRIVER_IIC_H
#define  __DRIVER_IIC_H
#include "config.h"

int IICDriverInit(void);
int IICDriverWrite(uint16_t DevAddress, uint8_t *pData, uint16_t Size);
int IICDriverRead(uint16_t DevAddress, uint8_t *pData, uint16_t Size);


#endif /* __DRIVER_IIC_H */

