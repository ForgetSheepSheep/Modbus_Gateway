#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* 4G模块串口配置 */
#define FOUR_G_UART_BAUDRATE        115200u

/* MQTT服务器配置：用户根据实际平台修改 */
#define MQTT_HOST                   "your_mqtt_host"
#define MQTT_PORT                   1883u
#define MQTT_CLIENT_ID              "modbus_gateway_001"
#define MQTT_USERNAME               "your_username"
#define MQTT_PASSWORD               "your_password"

/* MQTT Topic配置 */
#define MQTT_TOPIC_PUB              "device/modbus_gateway/telemetry"
#define MQTT_TOPIC_SUB              "device/modbus_gateway/cmd"

/* MQTT测试上报内容 */
#define MQTT_HELLO_PAYLOAD          "{\"msg\":\"hello_4g\",\"fw_ver\":\"1.0.0\"}"

#endif /* __APP_CONFIG_H */
