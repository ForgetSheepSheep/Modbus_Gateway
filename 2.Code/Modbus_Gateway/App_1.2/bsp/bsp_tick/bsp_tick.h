#ifndef  __BSP_TICK_H
#define  __BSP_TICK_H
#include "config.h"

void BspTickInit(void);
uint32_t BspGetTick(void);
void BspCallbackRegister(void (*fun)(void));

#endif /* __BSP_TICK_H */
