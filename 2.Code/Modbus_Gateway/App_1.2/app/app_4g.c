#include "app_4g.h"
#include "./component_4g/component_4g.h"
#include "./driver_4g_uart/driver_4g_uart.h"
#include "./bsp_tick/bsp_tick.h"
#include "./cJSON/cJSON.h"
#include "./component_ota/component_ota.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_4G_AT_BUF_LEN       512
#define APP_4G_LINE_BUF_LEN     1024
#define APP_4G_INIT_STEP_NUM    (sizeof(g_4g_init_steps) / sizeof(g_4g_init_steps[0]))

#define APP_4G_AT_TEST          "AT\r\n"
#define APP_4G_AT_MQTT_MULTI    "AT+MQMULTEN=1\r\n"
#define APP_4G_AT_MQTT_FILTER   "AT+MQTTFILTER=0\r\n"
#define APP_4G_AT_RESET         "AT+REST\r\n"
#define APP_4G_EXPECT_OK        "OK\r\n"
#define APP_4G_EXPECT_REST_OK   APP_4G_EXPECT_OK

typedef struct
{
    char *cmd;
    char *expect;
    uint32_t timeout_ms;
} App4GInitStep_t;

static App4GInitStep_t g_4g_init_steps[] =
{
    {APP_4G_AT_TEST,        APP_4G_EXPECT_OK,      3000},
    {APP_4G_AT_MQTT_MULTI,  APP_4G_EXPECT_OK,      3000},
    {APP_4G_AT_MQTT_FILTER, APP_4G_EXPECT_OK,      3000},
    {"AT+MQSUBM=0,1,0,4,\"" APP_4G_SUB_RPC "\"\r\n",  APP_4G_EXPECT_OK, 3000},
    {"AT+MQSUBM=1,1,0,4,\"" APP_4G_SUB_ATTR "\"\r\n", APP_4G_EXPECT_OK, 3000},
    {"AT+MQSUBM=2,1,0,4,\"" APP_4G_SUB_OTA "\"\r\n",  APP_4G_EXPECT_OK, 3000},
    {APP_4G_AT_RESET,       APP_4G_EXPECT_REST_OK, 10000},
};

static uint8_t g_4g_ready = 0;
static uint8_t g_4g_init_index = 0;
static uint8_t g_4g_init_started = 0;
static uint8_t g_4g_cmd_pending = 0;
static App4GCmd_t g_4g_last_cmd;
static char g_4g_at_cmd_buf[APP_4G_AT_BUF_LEN];

static int App4GJsonItemToUint8(cJSON *item, uint8_t *value);
static void App4GProcessInit(void);
static void App4GProcessIncoming(void);

void App4GInit(void)
{
    Component4GInit();
    g_4g_ready = 0;
    g_4g_init_index = 0;
    g_4g_init_started = 0;
    g_4g_cmd_pending = 0;
    memset(&g_4g_last_cmd, 0, sizeof(g_4g_last_cmd));
    printf("[4G] init start\r\n");
}

void App4GProcess(void)
{
    if(g_4g_ready == 0)
    {
        App4GProcessInit();
        return;
    }

    App4GProcessIncoming();
}

uint8_t App4GIsReady(void)
{
    return g_4g_ready;
}

uint8_t App4GGetCmd(App4GCmd_t *cmd)
{
    if(cmd == NULL)
    {
        return 0;
    }

    if(g_4g_cmd_pending == 0)
    {
        return 0;
    }

    *cmd = g_4g_last_cmd;
    g_4g_cmd_pending = 0;

    return 1;
}

uint8_t App4GPushData(const char *topic, const char *json_data)
{
    int len = 0;

    if(topic == NULL || json_data == NULL)
    {
        return EFAIL;
    }

    len = snprintf(g_4g_at_cmd_buf, sizeof(g_4g_at_cmd_buf), "MQPUB,1,%s,%s", topic, json_data);
    if(len < 0 || len >= (int)sizeof(g_4g_at_cmd_buf))
    {
        printf("[4G] publish data too long\r\n");
        return EFAIL;
    }

    Component4GSendString(g_4g_at_cmd_buf);

    return ESUCCESS;
}

uint8_t App4GPullData(uint8_t *str, App4GCmd_t *cmd)
{
    cJSON *root = NULL;
    cJSON *params = NULL;
    cJSON *addr_item = NULL;
    cJSON *ctrl_item = NULL;

    if(str == NULL || cmd == NULL)
    {
        return EFAIL;
    }

    root = cJSON_Parse((char *)str);
    if(root == NULL)
    {
        printf("[4G] json parse failed\r\n");
        return EFAIL;
    }

    addr_item = cJSON_GetObjectItem(root, "addr");
    ctrl_item = cJSON_GetObjectItem(root, "ctrl");

    if(addr_item == NULL || ctrl_item == NULL)
    {
        params = cJSON_GetObjectItem(root, "params");
        if(params != NULL && cJSON_IsObject(params))
        {
            if(addr_item == NULL)
            {
                addr_item = cJSON_GetObjectItem(params, "addr");
            }
            if(ctrl_item == NULL)
            {
                ctrl_item = cJSON_GetObjectItem(params, "ctrl");
            }
        }
    }

    if(App4GJsonItemToUint8(addr_item, &cmd->addr) != 0)
    {
        printf("[4G] missing addr\r\n");
        cJSON_Delete(root);
        return EFAIL;
    }

    if(App4GJsonItemToUint8(ctrl_item, &cmd->ctrl) != 0)
    {
        printf("[4G] missing ctrl\r\n");
        cJSON_Delete(root);
        return EFAIL;
    }

    cJSON_Delete(root);
    return ESUCCESS;
}

static void App4GProcessInit(void)
{
    Component4GCmdState_t state;

    if(g_4g_init_index >= APP_4G_INIT_STEP_NUM)
    {
        g_4g_ready = 1;
        printf("[4G] init ok\r\n");
        return;
    }

    if(g_4g_init_started == 0)
    {
        if(Component4GSendCmdStart(g_4g_init_steps[g_4g_init_index].cmd,
                                   g_4g_init_steps[g_4g_init_index].expect,
                                   g_4g_init_steps[g_4g_init_index].timeout_ms) == 0)
        {
            return;
        }
        g_4g_init_started = 1;
        return;
    }

    state = Component4GSendCmdProcess();
    if(state == COMPONENT_4G_CMD_OK)
    {
        g_4g_init_index++;
        g_4g_init_started = 0;
    }
    else if(state == COMPONENT_4G_CMD_TIMEOUT || state == COMPONENT_4G_CMD_ERROR)
    {
        printf("[4G] step %u failed state=%u expect=%s, retry\r\n",
               g_4g_init_index,
               state,
               g_4g_init_steps[g_4g_init_index].expect);
        g_4g_init_started = 0;
    }
}

/************************************************************
 * @brief Process incoming 4G UART data
 *        Handles: RPC JSON commands, OTA chunk binary data,
 *        and attributes OTA notification
 ************************************************************/
static void App4GProcessIncoming(void)
{
    static uint8_t line_buf[APP_4G_LINE_BUF_LEN];
    static uint16_t line_pos = 0;
    static uint16_t brace_count = 0;
    static uint8_t in_json = 0;
    static uint8_t in_ota_chunk = 0;
    static uint32_t ota_bytes_remaining = 0;
    static uint8_t ota_data_buf[512];
    static uint16_t ota_data_pos = 0;
    uint8_t ch = 0;

    while(Drv4GUARTReadByte(&ch) == ESUCCESS)
    {
        /* OTA binary chunk receive mode */
        if(in_ota_chunk != 0)
        {
            if(ota_data_pos < sizeof(ota_data_buf))
            {
                ota_data_buf[ota_data_pos++] = ch;
            }
            ota_bytes_remaining--;

            if(ota_bytes_remaining == 0)
            {
                ComponentOtaFeedChunk(ota_data_buf, ota_data_pos);
                in_ota_chunk = 0;
                ota_data_pos = 0;
                line_pos = 0;
                line_buf[0] = '\0';
                in_json = 0;
                brace_count = 0;
            }
            continue;
        }

        if(line_pos < (APP_4G_LINE_BUF_LEN - 1))
        {
            line_buf[line_pos++] = ch;
            line_buf[line_pos] = '\0';
        }

        /* Detect OTA chunk response topic: v2/fw/response/{id}/chunk/{id},{bytes},{data} */
        if(line_pos > 30 && in_json == 0)
        {
            char *fw_resp = strstr((char *)line_buf, "v2/fw/response/");
            if(fw_resp != NULL)
            {
                char *chunk_str = strstr(fw_resp, "/chunk/");
                if(chunk_str != NULL)
                {
                    char *comma = strchr(chunk_str + 7, ',');
                    if(comma != NULL)
                    {
                        uint32_t ota_bytes = (uint32_t)strtoul(comma + 1, NULL, 10);
                        if(ota_bytes > 0 && ota_bytes <= 256)
                        {
                            char *last_comma = strrchr((char *)line_buf, ',');
                            if(last_comma != NULL)
                            {
                                uint16_t data_start = (uint16_t)(last_comma - (char *)line_buf + 1);
                                uint16_t already = line_pos - data_start;

                                ota_data_pos = 0;
                                if(already > 0)
                                {
                                    memcpy(ota_data_buf, &line_buf[data_start], already);
                                    ota_data_pos = already;
                                }

                                ota_bytes_remaining = ota_bytes - already;
                                if(ota_bytes_remaining > 0)
                                {
                                    in_ota_chunk = 1;
                                }
                                else
                                {
                                    ComponentOtaFeedChunk(ota_data_buf, ota_data_pos);
                                    in_ota_chunk = 0;
                                    ota_data_pos = 0;
                                    line_pos = 0;
                                    line_buf[0] = '\0';
                                }
                                continue;
                            }
                        }
                    }
                }
            }
        }

        /* JSON brace matching */
        if(ch == '{')
        {
            if(in_json == 0)
            {
                in_json = 1;
                brace_count = 1;
            }
            else
            {
                brace_count++;
            }
        }
        else if(ch == '}' && in_json != 0)
        {
            if(brace_count > 0)
            {
                brace_count--;
            }

            if(brace_count == 0)
            {
                char *json_start = strchr((char *)line_buf, '{');
                in_json = 0;

                if(json_start != NULL)
                {
                    /* Check for attributes OTA notification */
                    if(strstr((char *)line_buf, "v1/devices/me/attributes") != NULL)
                    {
                        if(strstr(json_start, "fw_title") != NULL)
                        {
                            ComponentOtaParseNotify((uint8_t *)json_start);
                            ComponentOtaTrigger();
                        }
                    }

                    /* Normal RPC command parsing */
                    App4GCmd_t cmd;
                    if(App4GPullData((uint8_t *)json_start, &cmd) == ESUCCESS)
                    {
                        g_4g_last_cmd = cmd;
                        g_4g_cmd_pending = 1;
                    }
                }

                line_pos = 0;
                line_buf[0] = '\0';
            }
        }

        if(line_pos >= (APP_4G_LINE_BUF_LEN - 1))
        {
            line_pos = 0;
            line_buf[0] = '\0';
            brace_count = 0;
            in_json = 0;
            in_ota_chunk = 0;
            ota_data_pos = 0;
            ota_bytes_remaining = 0;
        }
    }
}

static int App4GJsonItemToUint8(cJSON *item, uint8_t *value)
{
    unsigned long parsed = 0;

    if(item == NULL || value == NULL)
    {
        return -1;
    }

    if(cJSON_IsNumber(item))
    {
        parsed = (unsigned long)item->valueint;
    }
    else if(cJSON_IsString(item) && item->valuestring != NULL)
    {
        parsed = strtoul(item->valuestring, NULL, 0);
    }
    else
    {
        return -1;
    }

    if(parsed > 0xFF)
    {
        return -1;
    }

    *value = (uint8_t)parsed;

    return 0;
}
