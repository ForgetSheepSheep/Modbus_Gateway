#ifndef __APP_PARAM_H
#define __APP_PARAM_H

#include "config.h"

#define APP_PARAM_DEFAULT_SLAVE_ADDR    0x03U
#define APP_PARAM_MIN_SLAVE_ADDR        1U
#define APP_PARAM_MAX_SLAVE_ADDR        247U

void AppParamInit(void);
uint8_t AppParamGetSlaveAddr(void);
int AppParamSetSlaveAddr(uint8_t slaveAddr);
uint8_t AppParamIsValidSlaveAddr(uint8_t slaveAddr);

#endif /* __APP_PARAM_H */
