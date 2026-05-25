#ifndef __APP_MODBUS_MASTER_H
#define __APP_MODBUS_MASTER_H

#include "config.h"
#include "app_4g.h"

#define APP_MODBUS_BAUDRATE    9600U
#define APP_MODBUS_PORT        1U
#define APP_MODBUS_PARITY_NONE 0U
#define APP_MODBUS_REPORT_PERIOD_MS 5000U

void AppModbusMasterInit(void);
void AppModbusMasterProcess(void);
uint8_t AppModbusMasterControl(const App4GCmd_t *cmd);

#endif /* __APP_MODBUS_MASTER_H */
