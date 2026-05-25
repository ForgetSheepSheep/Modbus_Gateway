#ifndef __DRIVER_KEY_H
#define __DRIVER_KEY_H

#include "config.h"

#define FALSE 0
#define TRUE  1
#define SHORT_VALUE 0x10

uint8_t KeyDriverGetValue(uint8_t index);
uint8_t KeyDriverReadState(uint8_t index);
int KeyDriverInit(void);

#endif /* __DRIVER_KEY_H */
