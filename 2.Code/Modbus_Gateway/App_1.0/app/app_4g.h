#ifndef __APP_4G_H
#define __APP_4G_H

#include "config.h"

#define SLAVE_ADDR_ADC      0x01
#define SLAVE_ADDR_IO       0x02
#define SLAVE_ADDR_HT       0x03
#define SLAVE_ADDR_IO_KEY   SLAVE_ADDR_IO
#define SLAVE_ADDR_TH       SLAVE_ADDR_HT

#define REG_OUTPUT_1        0U
#define REG_OUTPUT_2        1U
#define REG_OUTPUT_3        2U
#define REG_OUTPUT_4        3U
#define REG_OUTPUT_5        4U
#define REG_ADC_PA1         5U
#define REG_ADC_PA2         6U
#define REG_ADC_SLAVE_ADDR  7U
#define REG_IO_KEY1         5U
#define REG_IO_KEY2         6U
#define REG_IO_KEY3         7U
#define REG_IO_SLAVE_ADDR   8U
#define REG_HT_TEMP_X10     5U
#define REG_HT_HUMI_X10     6U
#define REG_HT_SLAVE_ADDR   7U

#define APP_MODBUS_OUTPUT_REG_START  REG_OUTPUT_1
#define APP_MODBUS_OUTPUT_REG_COUNT  5U
#define APP_MODBUS_OUTPUT_ALL_MASK   0x1FU

#define REG_IO_STATE        REG_OUTPUT_1
#define REG_KEY_STATE       REG_IO_KEY1
#define REG_TEMP            REG_HT_TEMP_X10
#define REG_HUMI            REG_HT_HUMI_X10
#define REG_ADC1            REG_ADC_PA1
#define REG_ADC2            REG_ADC_PA2

#define KEY1_PRESS          0x01
#define KEY2_PRESS          0x02
#define KEY3_PRESS          0x04
#define KEY_ALL_MASK        0x07

#define APP_4G_SUB_RPC      "v1/devices/me/rpc/request/+"
#define APP_4G_SUB_ATTR     "v1/devices/me/attributes"
#define APP_4G_PUB_TELEMETRY "v1/devices/me/telemetry"
#define APP_4G_PUB_RPC_RESP "v1/devices/me/rpc/response/"
#define APP_4G_SUB_OTA      "v2/fw/response/+/chunk/#"
#define APP_4G_PUB_OTA      "v2/fw/request/+/chunk/#"

typedef struct
{
    uint8_t addr;
    uint8_t ctrl;
} App4GCmd_t;

void App4GInit(void);
void App4GProcess(void);
uint8_t App4GIsReady(void);
uint8_t App4GGetCmd(App4GCmd_t *cmd);
uint8_t App4GPushData(const char *topic, const char *json_data);
uint8_t App4GPullData(uint8_t *str, App4GCmd_t *cmd);

#endif /* __APP_4G_H */
