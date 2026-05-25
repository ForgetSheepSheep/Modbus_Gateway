#ifndef  __APP_TASK_H
#define  __APP_TASK_H
#include <stdint.h>

void AppTaskInit(void);
void AppTaskLoop(void);
uint16_t AppTaskGetAHT20Temp(void);
uint16_t AppTaskGetAHT20Humi(void);

#endif /* __APP_TASK_H */

