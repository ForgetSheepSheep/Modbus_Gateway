#ifndef  __DRIVER_SYSTICK_H
#define  __DRIVER_SYSTICK_H
#include "config.h"


void SysTickInit(void);
void SysTickRegisterCallback(void (*cb)(void));
uint32_t SysTickGetTick(void);

#endif  /* __DRIVER_SYSTICK_H */
