#ifndef _PORT_TIMER_H_
#define _PORT_TIMER_H_

#include "port_macro.h"

void ModbusMasterPortTimerInit(USHORT usTim1Timerout50us);
void ModbusMasterPortTimerEnable(void);
void ModbusMasterPortTimerDisable(void);

void ModbusMasterTimerExpiredISR(void);

#endif /* _PORT_TIMER_H_ */
