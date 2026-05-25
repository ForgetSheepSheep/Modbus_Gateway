#include "app_modbus_master.h"
#include "modbus_rtu.h"
#include "./bsp_tick/bsp_tick.h"
#include <stdio.h>

#define APP_MODBUS_RX_FRAME_LEN  MODBUS_RTU_ADU_MAX_SIZE
#define APP_MODBUS_REPORT_JSON_LEN 384U

typedef enum
{
    APP_MODBUS_REQ_NONE = 0,
    APP_MODBUS_REQ_CONTROL,
    APP_MODBUS_REQ_READ_REPORT
} AppModbusReqType_t;

typedef struct
{
    uint8_t slave_addr;
    USHORT reg_count;
} AppModbusReadItem_t;

typedef struct
{
    uint8_t valid;
    USHORT reg[9];
} AppModbusReadData_t;

static uint8_t g_modbus_busy = 0;
static AppModbusReqType_t g_modbus_req_type = APP_MODBUS_REQ_NONE;
static uint32_t g_modbus_last_report_tick = 0;
static uint8_t g_modbus_report_active = 0;
static uint8_t g_modbus_read_index = 0;
static uint8_t g_modbus_next_report_index = 0;
static AppModbusReadData_t g_modbus_report_data[3];
static UCHAR g_modbus_rx_frame[APP_MODBUS_RX_FRAME_LEN];
static char g_modbus_report_json[APP_MODBUS_REPORT_JSON_LEN];

static const AppModbusReadItem_t g_modbus_read_items[] =
{
    { SLAVE_ADDR_IO,  9U },
    { SLAVE_ADDR_ADC, 8U },
    { SLAVE_ADDR_HT,  8U },
};

static uint8_t AppModbusIsSupportedSlave(uint8_t slave_addr);
static void AppModbusBuildOutputRegs(uint8_t ctrl, USHORT *regs, uint8_t reg_count);
static void AppModbusReportProcess(void);
static void AppModbusReportStart(void);
static void AppModbusReportReadNext(void);
static void AppModbusHandleReadResponse(const UCHAR *frame, USHORT len);
static void AppModbusPublishReport(void);

void AppModbusMasterInit(void)
{
    ModbusMasterRTUInit(APP_MODBUS_PORT, APP_MODBUS_BAUDRATE, APP_MODBUS_PARITY_NONE);
    g_modbus_busy = 0;
    printf("[Modbus] master init ok\r\n");
}

void AppModbusMasterProcess(void)
{
    Modbus_Master_state_t state;
    USHORT len;
    uint8_t request_done = 0;

    ModbusMasterPoll();

    state = ModbusMasterGetRequestState();
    if(state == MB_STATE_RX_SUCESS)
    {
        len = ModbusMasterGetRequestFrame(g_modbus_rx_frame);
        if(g_modbus_req_type == APP_MODBUS_REQ_READ_REPORT)
        {
            AppModbusHandleReadResponse(g_modbus_rx_frame, len);
        }
        g_modbus_busy = 0;
        request_done = 1;
        printf("[Modbus] response ok, len=%u\r\n", len);
    }
    else if(state == MB_STATE_RX_ERR)
    {
        g_modbus_busy = 0;
        request_done = 1;
        printf("[Modbus] response error\r\n");
    }
    else if(state == MB_STATE_RX_TIMEOUT)
    {
        g_modbus_busy = 0;
        request_done = 1;
        printf("[Modbus] response timeout\r\n");
    }

    if(request_done != 0)
    {
        g_modbus_req_type = APP_MODBUS_REQ_NONE;

        if(g_modbus_report_active != 0)
        {
            g_modbus_report_active = 0;
            AppModbusPublishReport();
            g_modbus_next_report_index++;
            if(g_modbus_next_report_index >= (uint8_t)(sizeof(g_modbus_read_items) / sizeof(g_modbus_read_items[0])))
            {
                g_modbus_next_report_index = 0;
            }
        }
    }

    AppModbusReportProcess();
}

uint8_t AppModbusMasterControl(const App4GCmd_t *cmd)
{
    if(cmd == NULL)
    {
        return EFAIL;
    }

    if(g_modbus_busy != 0)
    {
        printf("[Modbus] busy\r\n");
        return EFAIL;
    }

    if(AppModbusIsSupportedSlave(cmd->addr) == 0)
    {
        printf("[Modbus] unsupported control addr=0x%02x\r\n", cmd->addr);
        return EFAIL;
    }

    {
        USHORT regs[APP_MODBUS_OUTPUT_REG_COUNT];

        AppModbusBuildOutputRegs(cmd->ctrl, regs, APP_MODBUS_OUTPUT_REG_COUNT);
        ModbusMasterWriteMultipleRegRequest(cmd->addr,
                                            APP_MODBUS_OUTPUT_REG_START,
                                            APP_MODBUS_OUTPUT_REG_COUNT,
                                            regs);
    }

    g_modbus_busy = 1;
    g_modbus_req_type = APP_MODBUS_REQ_CONTROL;
    printf("[Modbus] write addr=0x%02x regs=%u-%u ctrl=0x%02x\r\n",
           cmd->addr,
           APP_MODBUS_OUTPUT_REG_START,
           APP_MODBUS_OUTPUT_REG_START + APP_MODBUS_OUTPUT_REG_COUNT - 1U,
           cmd->ctrl & APP_MODBUS_OUTPUT_ALL_MASK);

    return ESUCCESS;
}

static uint8_t AppModbusIsSupportedSlave(uint8_t slave_addr)
{
    switch(slave_addr)
    {
        case SLAVE_ADDR_ADC:
        case SLAVE_ADDR_IO:
        case SLAVE_ADDR_HT:
            return 1;

        default:
            return 0;
    }
}

static void AppModbusBuildOutputRegs(uint8_t ctrl, USHORT *regs, uint8_t reg_count)
{
    uint8_t i;

    if(regs == NULL)
    {
        return;
    }

    for(i = 0; i < reg_count; i++)
    {
        regs[i] = ((ctrl & (1U << i)) != 0U) ? OPEN : CLOSE;
    }
}

static void AppModbusReportProcess(void)
{
    uint32_t now = BspGetTick();

    if(App4GIsReady() == 0)
    {
        g_modbus_last_report_tick = now;
        return;
    }

    if(g_modbus_report_active != 0 || g_modbus_busy != 0)
    {
        return;
    }

    if((now - g_modbus_last_report_tick) >= APP_MODBUS_REPORT_PERIOD_MS)
    {
        g_modbus_last_report_tick = now;
        AppModbusReportStart();
    }
}

static void AppModbusReportStart(void)
{
    g_modbus_read_index = g_modbus_next_report_index;
    g_modbus_report_data[g_modbus_read_index].valid = 0;
    memset(g_modbus_report_data[g_modbus_read_index].reg, 0, sizeof(g_modbus_report_data[g_modbus_read_index].reg));

    g_modbus_report_active = 1;
    AppModbusReportReadNext();
}

static void AppModbusReportReadNext(void)
{
    if(g_modbus_read_index >= (uint8_t)(sizeof(g_modbus_read_items) / sizeof(g_modbus_read_items[0])))
    {
        g_modbus_report_active = 0;
        return;
    }

    if(g_modbus_busy != 0)
    {
        return;
    }

    ModbusMasterReadHoldingRegRequest(g_modbus_read_items[g_modbus_read_index].slave_addr,
                                      0U,
                                      g_modbus_read_items[g_modbus_read_index].reg_count);
    g_modbus_busy = 1;
    g_modbus_req_type = APP_MODBUS_REQ_READ_REPORT;

}

static void AppModbusHandleReadResponse(const UCHAR *frame, USHORT len)
{
    uint8_t i;
    uint8_t byte_count;
    AppModbusReadData_t *data = NULL;

    if(frame == NULL || g_modbus_read_index >= (uint8_t)(sizeof(g_modbus_read_items) / sizeof(g_modbus_read_items[0])))
    {
        return;
    }

    if(len < 5U ||
       frame[0] != g_modbus_read_items[g_modbus_read_index].slave_addr ||
       frame[1] != MB_FUNC_READ_HOLDING_REGISTER)
    {
        printf("[Modbus] read frame mismatch len=%u rx_addr=0x%02x func=0x%02x\r\n",
               len,
               (len > 0U) ? frame[0] : 0U,
               (len > 1U) ? frame[1] : 0U);
        return;
    }

    byte_count = frame[2];
    if(byte_count != (uint8_t)(g_modbus_read_items[g_modbus_read_index].reg_count * 2U) ||
       len < (USHORT)(3U + byte_count + 2U))
    {
        printf("[Modbus] read byte count err byte_count=%u len=%u expect=%u\r\n",
               byte_count,
               len,
               (uint8_t)(g_modbus_read_items[g_modbus_read_index].reg_count * 2U));
        return;
    }

    data = &g_modbus_report_data[g_modbus_read_index];
    data->valid = 1;

    for(i = 0; i < g_modbus_read_items[g_modbus_read_index].reg_count; i++)
    {
        data->reg[i] = ((USHORT)frame[3U + i * 2U] << 8) | frame[4U + i * 2U];
    }
}

static void AppModbusPublishReport(void)
{
    AppModbusReadData_t *data = NULL;
    uint16_t io_state = 0;
    int len;

    if(g_modbus_read_index >= (uint8_t)(sizeof(g_modbus_read_items) / sizeof(g_modbus_read_items[0])))
    {
        return;
    }

    data = &g_modbus_report_data[g_modbus_read_index];

    switch(g_modbus_read_items[g_modbus_read_index].slave_addr)
    {
        case SLAVE_ADDR_ADC:
            len = snprintf(g_modbus_report_json,
                           sizeof(g_modbus_report_json),
                           "{\"addr\":\"01\",\"adc1\":%u,\"adc2\":%u}",
                           data->reg[5],
                           data->reg[6]);
            break;

        case SLAVE_ADDR_IO:
            io_state = (data->reg[0] ? 0x01U : 0U) |
                       (data->reg[1] ? 0x02U : 0U) |
                       (data->reg[2] ? 0x04U : 0U) |
                       (data->reg[3] ? 0x08U : 0U) |
                       (data->reg[4] ? 0x10U : 0U) |
                       (data->reg[5] ? 0x20U : 0U) |
                       (data->reg[6] ? 0x40U : 0U) |
                       (data->reg[7] ? 0x80U : 0U);
            len = snprintf(g_modbus_report_json,
                           sizeof(g_modbus_report_json),
                           "{\"addr\":\"02\",\"io\":%u}",
                           io_state);
            break;

        case SLAVE_ADDR_HT:
            len = snprintf(g_modbus_report_json,
                           sizeof(g_modbus_report_json),
                           "{\"addr\":\"03\",\"temp\":%d,\"humi\":%u}",
                           (int16_t)data->reg[5],
                           data->reg[6]);
            break;

        default:
            return;
    }

    if(len < 0 || len >= (int)sizeof(g_modbus_report_json))
    {
        printf("[Modbus] report json too long\r\n");
        return;
    }

    App4GPushData(APP_4G_PUB_TELEMETRY, g_modbus_report_json);
    printf("[REPORT] %s\r\n", g_modbus_report_json);
}
