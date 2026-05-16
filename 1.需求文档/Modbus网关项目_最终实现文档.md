# GD32F303 Modbus 网关项目最终实现文档

> 本文档为 Modbus 网关项目的最终整合版，涵盖从 V1 裸机到 V2 FreeRTOS 到 V3 RT-Thread 的完整实现方案。
> 项目目标：基于 GD32F303ZET6 实现一个带 Bootloader、串口升级、OTA、Modbus RTU 主站、4G MQTT 上云、参数掉电保存，并且可迁移 FreeRTOS / RT-Thread 的工业网关。

---

## 1. 硬件平台

### 1.1 芯片型号

```text
主力平台：GD32F303ZET6（Cortex-M3，512KB Flash，64KB SRAM）
兼容/对照平台：STM32F103ZET6
```

### 1.2 硬件资源表

| 模块 | 外设 | 引脚 | 作用 |
|---|---|---|---|
| AT24C02 | 软件 I2C | SCL/SDA | 保存升级参数和系统配置 |
| GD25Q128 / W25Q64 | SPI0 | PB13(SCK)/PB14(MISO)/PB15(MOSI)/PE2(CS) | 保存 OTA 固件和备份 |
| RS485 | USARTx + GPIO | TX/RX/DE | Modbus RTU 通信 |
| 4G 模块 | USARTx | TX/RX | MQTT 上云 |
| 调试串口 | USARTx | TX/RX | printf 调试 |
| LED | GPIO | - | 状态指示 |
| 按键 | GPIO | - | 人机交互 |

### 1.3 平台兼容宏

```c
#define MCU_PLATFORM_GD32F303    1
#define MCU_PLATFORM_STM32F103   2

#define MCU_PLATFORM             MCU_PLATFORM_GD32F303
```

APP 层、MID 层、Component 层尽量通用，Driver 层按平台分开适配。GD32F303 使用 GD32 标准库（FMC 擦写 Flash），STM32F103 使用 HAL/标准库（FLASH 擦写）。

### 1.4 硬件确认清单

开发前必须确认：

- [ ] 主控型号：GD32F303 具体封装和 Flash / SRAM 容量
- [ ] AT24C02 引脚：I2C0_SCL / I2C0_SDA，A0/A1/A2 接 GND，7 位地址 0x50，写地址 0xA0，读地址 0xA1，WP 接 GND 允许写入
- [ ] GD25Q128 引脚：CS=PE2, SCK=PB13, MISO=PB14, MOSI=PB15
- [ ] GD25Q128 JEDEC ID：C8 40 18
- [ ] 调试串口使用哪个 USART
- [ ] RS485 串口使用哪个 USART，DE/RE 引脚
- [ ] 4G 模块串口使用哪个 USART
- [ ] 串口屏 / 上位机使用哪个 USART
- [ ] LED、按键、蜂鸣器等基本 IO

---

## 2. 工程目录结构

```text
GD32F303_Modbus_Gateway
├── 00_Docs
│   ├── 01_Hardware.md
│   ├── 02_Bootloader.md
│   ├── 03_Modbus.md
│   ├── 04_MQTT.md
│   ├── 05_OTA.md
│   ├── 06_FreeRTOS.md
│   ├── 07_Debug_Record.md
│   └── 08_Changelog.md
│
├── 01_Bootloader
│   ├── Project
│   └── User
│       ├── config
│       │   └── boot_config.h
│       ├── driver
│       │   ├── driver_usart.c/h
│       │   ├── driver_iic.c/h
│       │   ├── driver_at24c02.c/h
│       │   ├── driver_spi.c/h
│       │   ├── driver_gd25q128.c/h
│       │   ├── driver_flash.c/h
│       │   └── driver_led.c/h
│       ├── component
│       │   ├── component_crc16.c/h
│       │   ├── component_crc32.c/h
│       │   └── component_ymodem.c/h
│       └── boot
│           ├── boot_main.c/h
│           ├── boot_jump.c/h
│           ├── boot_param.c/h
│           ├── boot_update.c/h
│           └── boot_app_check.c/h
│
├── 02_App_V1_BareMetal
│   ├── Project
│   └── User
│       ├── config
│       │   └── app_config.h
│       ├── driver
│       │   ├── driver_usart.c/h
│       │   ├── driver_ringbuffer.c/h
│       │   ├── driver_rs485.c/h
│       │   ├── driver_iic.c/h
│       │   ├── driver_at24c02.c/h
│       │   ├── driver_spi.c/h
│       │   ├── driver_gd25q128.c/h
│       │   └── driver_led.c/h
│       ├── component
│       │   ├── component_crc16.c/h
│       │   ├── component_crc32.c/h
│       │   ├── component_cjson.c/h
│       │   └── component_modbus.c/h
│       ├── mid
│       │   ├── mid_modbus_master.c/h
│       │   ├── mid_mqtt.c/h
│       │   ├── mid_storage.c/h
│       │   ├── mid_ota.c/h
│       │   └── mid_protocol.c/h
│       └── app
│           ├── app_main.c/h
│           ├── app_modbus.c/h
│           ├── app_mqtt.c/h
│           ├── app_hmi.c/h
│           ├── app_ota.c/h
│           ├── app_save.c/h
│           ├── app_monitor.c/h
│           ├── app_data_center.c/h
│           └── app_cmd_queue.c/h
│
├── 03_FreeRTOS_V2
├── 04_RTThread_V3
├── Tools
└── README.md
```

---

## 3. Flash 和存储规划

### 3.1 内部 Flash 分区

```text
0x08000000  ┌──────────────────────────┐
            │ Bootloader 区 32KB       │
0x08008000  ├──────────────────────────┤
            │ APP 运行区 480KB         │
            │ 从 0x08008000 开始       │
0x08080000  └──────────────────────────┘
```

```c
#define BOOT_START_ADDR          0x08000000U
#define BOOT_SIZE                (32U * 1024U)

#define APP_START_ADDR           0x08008000U
#define APP_MAX_SIZE             (480U * 1024U)
```

Keil 设置：
- Bootloader 工程：IROM1 Start = 0x08000000，Size = 0x8000
- APP 工程：IROM1 Start = 0x08008000，Size = 0x78000

### 3.2 AT24C02 参数区

```c
#define BOOT_PARAM_MAIN_ADDR      0x00U
#define BOOT_PARAM_BAK_ADDR       0x80U
#define APP_PARAM_MAIN_ADDR       0x20U
#define APP_PARAM_BAK_ADDR        0xA0U
```

AT24C02 保存：
- 升级标志、APP 大小、APP CRC32、APP 版本号
- Modbus 参数、系统配置

### 3.3 GD25Q128 / W25Q64 分区

```c
#define EXT_FW_DOWNLOAD_ADDR       0x000000UL
#define EXT_FW_BACKUP_ADDR         0x100000UL
#define EXT_UPDATE_LOG_ADDR        0x200000UL

#define EXT_FW_AREA_SIZE           (512UL * 1024UL)
#define EXT_BACKUP_AREA_SIZE       (512UL * 1024UL)
```

---

## 4. 关键数据结构

### 4.1 BootParam_t

```c
#define BOOT_PARAM_MAGIC          0x5AA5C33CU
#define BOOT_PARAM_TAIL_MAGIC     0xA5A5C3C3U

#define UPDATE_FLAG_NONE          0x00U
#define UPDATE_FLAG_ENABLE        0x5AU
#define UPDATE_FLAG_DONE          0xA5U
#define UPDATE_FLAG_FAIL          0xF0U

#define BACKUP_FLAG_INVALID       0x00U
#define BACKUP_FLAG_VALID         0xA5U

typedef struct
{
    uint32_t magic_word;          /* 参数有效标志 */

    uint8_t  update_flag;         /* 是否需要升级 */
    uint8_t  update_mode;         /* 升级方式：1串口 2WiFi 3:4G */
    uint8_t  update_status;       /* 当前升级状态 */
    uint8_t  backup_valid;        /* 备份是否有效 */

    uint32_t app_size;            /* 当前 APP 大小 */
    uint32_t app_crc32;           /* 当前 APP CRC32 */
    uint32_t app_version;         /* 当前 APP 版本 */

    uint32_t new_fw_size;         /* 新固件大小 */
    uint32_t new_fw_crc32;        /* 新固件 CRC32 */
    uint32_t new_fw_version;      /* 新固件版本 */

    uint16_t boot_fail_count;     /* APP 启动失败次数 */
    uint16_t reserved0;

    uint32_t param_crc32;         /* 参数自身 CRC */
    uint32_t tail_magic;          /* 参数结束标志 */

} BootParam_t;
```

### 4.2 GatewayData_t

```c
#define GATEWAY_SLAVE_MAX       16
#define GATEWAY_REG_MAX         128
#define GATEWAY_COIL_MAX        64

typedef struct
{
    uint16_t hold_reg[GATEWAY_REG_MAX];
    uint16_t input_reg[GATEWAY_REG_MAX];
    uint8_t  coil[GATEWAY_COIL_MAX];

    uint8_t  slave_online[GATEWAY_SLAVE_MAX];
    uint16_t slave_error_cnt[GATEWAY_SLAVE_MAX];

    uint8_t  mqtt_online;
    uint8_t  g4_online;
    uint8_t  ota_status;

    uint32_t system_tick;

} GatewayData_t;
```

### 4.3 ModbusSlaveNode_t

```c
typedef struct
{
    uint8_t  slave_addr;
    uint16_t reg_start;
    uint16_t reg_num;
    uint16_t reg_buf[32];
    uint8_t  online;
    uint16_t error_cnt;
    uint32_t last_poll_tick;

} ModbusSlaveNode_t;
```

### 4.4 GatewayCmd_t

```c
typedef enum
{
    GATEWAY_CMD_NONE = 0,
    GATEWAY_CMD_WRITE_COIL,
    GATEWAY_CMD_WRITE_REG,
    GATEWAY_CMD_READ_REG,
} GatewayCmdType_e;

typedef struct
{
    GatewayCmdType_e cmd_type;
    uint8_t  slave_addr;
    uint16_t addr;
    uint16_t value;

} GatewayCmd_t;
```

### 4.5 FirmwareHeader_t

```c
#define FW_HEADER_MAGIC      0x46574D47U   /* GMWF */

typedef struct
{
    uint32_t magic;
    uint32_t header_size;
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint32_t fw_version;
    uint32_t target_addr;
    uint32_t reserved[10];

} FirmwareHeader_t;
```

---

## 5. Bootloader 实现

### 5.1 上电流程

```text
MCU 复位
    ↓
运行 0x08000000 Bootloader
    ↓
初始化基础外设：时钟、串口、IIC、SPI、LED
    ↓
读取 AT24C02 中 BootParam_t
    ↓
判断参数是否有效（magic + CRC32）
    ↓
判断是否需要升级
    ↓
如果需要升级：
    校验 GD25Q128 新固件 CRC32
    备份旧 APP 到 GD25Q128 备份区
    擦除内部 Flash APP 区
    分块搬运新 APP
    校验新 APP CRC32
    清除升级标志
    跳转 APP
如果不需要升级：
    检查 APP 合法性（栈顶 + 入口地址）
    跳转 APP
如果 APP 无效：
    等待串口 YMODEM 升级
```

### 5.2 文件职责

| 文件 | 函数 | 职责 |
|---|---|---|
| boot_main.c | `BootMainInit()` `BootMainProcess()` | 初始化外设、读取参数、判断升级、跳转 |
| boot_jump.c | `BootCheckAppValid()` `BootJumpToApp()` | 检查 APP 合法性、关闭中断、设置 MSP、跳转 |
| boot_param.c | `BootParamLoad()` `BootParamSave()` `BootParamCheck()` | AT24C02 参数读写和校验 |
| boot_update.c | `BootUpdateProcess()` `BootCopyNewFirmwareToApp()` | 固件校验、备份、擦除、搬运、回退 |
| component_ymodem.c | `BootYmodemUpdateProcess()` | 串口 YMODEM 升级 |
| component_crc32.c | `Crc32Calculate()` | CRC32 校验 |
| driver_flash.c | `FlashErase()` `FlashWrite()` | 内部 Flash 擦写 |

### 5.3 APP 合法性检查

```c
uint8_t BootCheckAppValid(uint32_t app_addr)
{
    uint32_t app_stack = *(volatile uint32_t *)app_addr;
    uint32_t app_entry = *(volatile uint32_t *)(app_addr + 4U);

    /* 栈顶地址必须在 SRAM 范围 */
    if((app_stack & 0x2FFE0000U) != 0x20000000U)
    {
        return 0;
    }

    /* 入口地址必须在 Flash 范围 */
    if((app_entry & 0xFF000000U) != 0x08000000U)
    {
        return 0;
    }

    return 1;
}
```

### 5.4 跳转 APP

```c
typedef void (*AppFunc_t)(void);

void BootJumpToApp(uint32_t app_addr)
{
    uint32_t app_stack = *(volatile uint32_t *)app_addr;
    uint32_t app_entry = *(volatile uint32_t *)(app_addr + 4U);
    AppFunc_t app_func;

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    __set_MSP(app_stack);

    app_func = (AppFunc_t)app_entry;
    app_func();
}
```

### 5.5 固件搬运流程

```text
读取 AT24C02 中 app_size 和 app_crc32
    ↓
计算 GD25Q128 固件下载区 CRC32
    ↓
与 AT24C02 记录值比对
    ↓
备份当前 APP 到 GD25Q128 备份区（可选）
    ↓
擦除内部 Flash APP 区
    ↓
分块从 GD25Q128 读取（256/512 字节一块）
    ↓
分块写入内部 Flash APP 区
    ↓
计算内部 Flash APP 区 CRC32
    ↓
与 AT24C02 记录值比对
    ↓
成功：清除 upgrade_flag，跳转 APP
失败：设置 upgrade_flag = FAIL，尝试从备份恢复
```

### 5.6 升级失败回退机制

```text
搬运新固件 CRC32 不匹配
    ↓
检查 backup_valid 标志
    ↓
如果备份有效：
    从 GD25Q128 备份区恢复旧 APP
    擦除 APP 区
    搬运备份固件
    校验 CRC32
    跳转旧 APP
如果备份无效：
    设置 APP 无效标志
    等待串口升级
```

### 5.7 调试输出示例

正常启动：

```text
BOOT RUNNING
Read param: magic=0x5AA5C33C, flag=0x00, size=0, crc=0x00000000
Check APP valid: OK
Jump to APP
APP RUNNING FROM 0x08008000
```

串口升级：

```text
BOOT RUNNING
Press U to update
[YMODEM] First packet: name=app.bin, size=86240
[YMODEM] Packet 1 OK
[YMODEM] Packet 2 OK
...
[YMODEM] EOT received
[YMODEM] CRC32 OK: 0x12345678
[YMODEM] Write param OK
[YMODEM] Reboot
BOOT RUNNING
Read param: flag=0x5A, size=86240, crc=0x12345678
UPDATE ENABLE
CHECK NEW FW OK
COPY FW TO APP OK
APP CRC OK
CLEAR FLAG
Jump to APP
```

---

## 6. YMODEM 串口升级

### 6.1 控制字符

```c
#define YMODEM_SOH      0x01    /* 128字节包 */
#define YMODEM_STX      0x02    /* 1024字节包 */
#define YMODEM_EOT      0x04    /* 传输结束 */
#define YMODEM_ACK      0x06    /* 接收正确 */
#define YMODEM_NAK      0x15    /* 接收错误 */
#define YMODEM_CAN      0x18    /* 取消传输 */
#define YMODEM_CRC      0x43    /* 字符 C，请求 CRC 模式 */
```

### 6.2 包格式

```text
SOH/STX + 包序号 + 包序号反码 + 数据区 + CRC16高 + CRC16低
```

- SOH：128 字节数据区
- STX：1024 字节数据区
- 第 0 包：文件名\0文件大小\0（不含实际固件数据）
- 第 1 包起：真正的 APP bin 数据

### 6.3 职责边界

```text
串口中断：只负责收字节，写入环形缓冲区
环形缓冲区：只负责缓存字节流
YMODEM 解析层：解析包头、包序号、CRC16、EOT
GD25Q128 驱动：只负责擦除、写入、读取
Bootloader 升级层：负责 CRC32 校验、搬运、回退、跳转
```

### 6.4 SecureCRT 操作流程

```text
1. 复位进入 Bootloader
2. 串口打印：Press U to update
3. 输入 U
4. Bootloader 发送字符 C
5. SecureCRT 选择 Transfer -> Send Ymodem
6. 选择 APP.bin
7. 等待传输完成
8. Bootloader 写入 GD25Q128 下载区
9. 校验 CRC32
10. 写 AT24C02 升级参数
11. 复位
12. Bootloader 搬运新 APP
```

---

## 7. 环形缓冲区驱动

### 7.1 结构体

```c
typedef struct
{
    uint8_t *buffer;
    uint16_t size;
    volatile uint16_t read;
    volatile uint16_t write;
} RingBuffer_t;
```

### 7.2 接口

```c
void RingBufferInit(RingBuffer_t *ring, uint8_t *buffer, uint16_t size);
uint8_t RingBufferWriteByte(RingBuffer_t *ring, uint8_t data);
uint8_t RingBufferReadByte(RingBuffer_t *ring, uint8_t *data);
uint16_t RingBufferGetLength(RingBuffer_t *ring);
void RingBufferClear(RingBuffer_t *ring);
```

环形缓冲区只负责保存数据，不负责解析协议。Modbus 解析、AT 指令解析、YMODEM 解析各自从 RingBuffer 读取数据。

---

## 8. RS485 驱动

### 8.1 接口

```c
void DrvRS485Init(void);
void DrvRS485SetTxMode(void);
void DrvRS485SetRxMode(void);
void DrvRS485Send(uint8_t *data, uint16_t len);
uint16_t DrvRS485Read(uint8_t *data, uint16_t max_len);
```

### 8.2 要求

- 发送前切发送模式（DE 拉高）
- 发送完成后切接收模式（DE 拉低）
- 接收数据进入 RingBuffer
- 发送完成可以用 USART TC 中断判断

---

## 9. Modbus RTU 协议

### 9.1 常用功能码

| 功能码 | 说明 |
|---|---|
| 0x03 | 读保持寄存器 |
| 0x04 | 读输入寄存器 |
| 0x05 | 写单个线圈 |
| 0x06 | 写单个保持寄存器 |
| 0x10 | 写多个保持寄存器 |

### 9.2 RTU 帧格式

```text
从机地址(1B) + 功能码(1B) + 数据(NB) + CRC16低(1B) + CRC16高(1B)
```

### 9.3 CRC16 计算

Modbus CRC16 使用多项式 0xA001（反转 0x8005），初始值 0xFFFF。

### 9.4 帧结束判断

Modbus RTU 一帧结束判断依赖串口空闲中断或 3.5 字符时间超时。第一版可以用超时方式：发送请求后，在一定时间内收集返回数据，然后解析。

### 9.5 Modbus 主站状态机

```c
typedef enum
{
    MODBUS_STATE_IDLE = 0,
    MODBUS_STATE_SEND_QUERY,
    MODBUS_STATE_WAIT_RESPONSE,
    MODBUS_STATE_PARSE_RESPONSE,
    MODBUS_STATE_TIMEOUT,
    MODBUS_STATE_ERROR,
} ModbusMasterState_e;
```

### 9.6 应答解析要点

- 判断从机地址是否匹配
- 判断功能码是否匹配（异常功能码 = 原功能码 + 0x80）
- 判断 CRC16 是否正确
- 解析读保持寄存器返回数据（字节数 + 数据）
- 解析写单线圈 / 单寄存器返回数据（回显原请求）
- 错误时增加错误计数
- 超时能识别

---

## 10. HMI 命令协议

### 10.1 调试串口命令格式

```text
coil 1 0 1        → 写 1 号从机 0 号线圈为 1
reg 1 10 1234     → 写 1 号从机 10 号寄存器为 1234
status            → 打印网关状态
```

### 10.2 数据上报格式

串口屏和上位机只负责显示数据和下发命令，不直接操作 Modbus。真正执行命令的是 AppModbusProcess。

---

## 11. APP 工程实现

### 7.1 APP 偏移设置

```c
#define APP_START_ADDR  0x08008000U

void AppVectorTableInit(void)
{
    SCB->VTOR = APP_START_ADDR;
}
```

必须放在 `main()` 初始化最前面。

### 7.2 软件分层

```text
App 层（业务逻辑）
├── app_main.c              系统主流程
├── app_modbus.c            Modbus 主站轮询
├── app_mqtt.c              MQTT 收发
├── app_hmi.c               串口屏/上位机交互
├── app_ota.c               OTA 下载
├── app_save.c              参数保存
├── app_monitor.c           看门狗和状态监测
├── app_data_center.c       数据中心
└── app_cmd_queue.c         命令队列

Mid 层（协议和中间件）
├── mid_modbus_master.c     Modbus 主站协议
├── mid_mqtt.c              MQTT 协议封装
├── mid_storage.c           存储管理
├── mid_ota.c               OTA 流程管理
└── mid_protocol.c          上位机协议

Component 层（通用组件）
├── component_crc16.c       CRC16 校验
├── component_crc32.c       CRC32 校验
├── component_cjson.c       JSON 解析
└── component_modbus.c      Modbus RTU 协议

Driver 层（硬件驱动）
├── driver_usart.c          串口驱动
├── driver_ringbuffer.c     环形缓冲区
├── driver_rs485.c          RS485 方向控制
├── driver_iic.c            软件 IIC
├── driver_at24c02.c        AT24C02 驱动
├── driver_spi.c            SPI 驱动
├── driver_gd25q128.c       GD25Q128 驱动
└── driver_led.c            LED 驱动
```

### 7.3 APP 主流程

```c
void AppMainInit(void)
{
    AppDataCenterInit();
    AppCmdQueueInit();
    AppModbusInit();
    AppMqttInit();
    AppOtaInit();
    AppSaveInit();
    AppMonitorInit();
}

void AppMainProcess(void)
{
    AppModbusProcess();
    AppMqttProcess();
    AppOtaProcess();
    AppSaveProcess();
    AppMonitorProcess();
}

int main(void)
{
    AppVectorTableInit();
    /* 系统时钟、外设初始化 */
    AppMainInit();

    while(1)
    {
        AppMainProcess();
    }
}
```

要求：
- 每个 Process 非阻塞，快速返回
- 不使用长 delay
- 串口接收使用环形缓冲区
- Modbus 总线只允许 AppModbusProcess 操作

---

## 8. Modbus 网关业务

### 8.1 命令队列原则

```text
MQTT 不直接操作 RS485
HMI 不直接操作 RS485
上位机不直接操作 RS485
都只写命令队列
AppModbusProcess 统一取命令并执行
```

裸机阶段用环形队列，FreeRTOS 阶段用 xQueue。

### 8.2 Modbus 从机轮询

```c
void AppModbusProcess(void)
{
    /* 1. 从命令队列取命令执行 */
    if(GatewayCmdQueueRead(&cmd) == 1)
    {
        MidModbusMasterExecuteCmd(&cmd);
    }

    /* 2. 按轮询表依次采集从机数据 */
    for(i = 0; i < slave_count; i++)
    {
        if(BspGetTick() - slave_node[i].last_poll_tick >= poll_interval)
        {
            MidModbusMasterPollSlave(&slave_node[i]);
            slave_node[i].last_poll_tick = BspGetTick();
        }
    }
}
```

### 8.3 Modbus 寄存器规划

| 地址 | 内容 |
|---|---|
| 40001~40032 | 从机 1 保持寄存器 |
| 40033~40064 | 从机 2 保持寄存器 |
| 30001~30032 | 从机 1 输入寄存器 |
| 30033~30064 | 从机 2 输入寄存器 |
| 00001~00064 | 线圈状态 |

---

## 9. 4G AT 框架和 MQTT

### 9.1 AT 指令框架

```c
typedef enum
{
    AT_STATE_IDLE = 0,
    AT_STATE_SEND,
    AT_STATE_WAIT_OK,
    AT_STATE_WAIT_DATA,
    AT_STATE_TIMEOUT,
    AT_STATE_DONE,
} AtState_e;

typedef struct
{
    AtState_e state;
    char *cmd;
    char *expect;
    uint32_t timeout_ms;
    uint32_t send_tick;
    uint8_t retry_count;
} AtContext_t;
```

### 9.2 MQTT 上传 JSON

```json
{
  "hold_reg_1": 100,
  "hold_reg_2": 200,
  "coil_1": 1,
  "slave_online": [1, 1, 0, 0],
  "mqtt": 1,
  "g4": 1
}
```

### 9.3 MQTT 下发命令

```json
{
  "method": "writeReg",
  "slave": 1,
  "addr": 40001,
  "value": 123
}
```

```json
{
  "method": "writeCoil",
  "slave": 1,
  "addr": 1,
  "value": 1
}
```

---

## 10. OTA 实现

### 10.1 APP OTA 职责

```text
1. 从 MQTT 收到升级信息（版本号、固件大小、CRC32、下载 URL）
2. 判断版本号是否高于当前版本
3. 分片下载固件
4. 写入 GD25Q128 下载区
5. 下载完成后计算 CRC32
6. CRC 正确后写 AT24C02 升级标志
7. 软件复位
```

### 10.2 Bootloader OTA 职责

```text
1. 读取 AT24C02 升级标志
2. 校验 GD25Q128 新固件
3. 备份旧 APP（可选）
4. 擦除内部 APP 区
5. 搬运新 APP
6. 校验新 APP CRC32
7. 清除升级标志
8. 跳转 APP
```

### 10.3 OTA 状态机

```c
typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_WAIT_INFO,
    OTA_STATE_ERASE_EXT_FLASH,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_CHECK_CRC,
    OTA_STATE_SET_BOOT_FLAG,
    OTA_STATE_REBOOT,
    OTA_STATE_FAIL,
} OtaState_e;
```

---

## 11. 参数保存

### 11.1 APP 参数结构体

```c
#define APP_PARAM_MAGIC       0xA55A2026U

typedef struct
{
    uint32_t magic;

    uint8_t  modbus_addr;
    uint8_t  baudrate_index;
    uint16_t poll_interval_ms;

    uint8_t  slave_count;
    uint8_t  mqtt_upload_enable;
    uint16_t upload_period_s;

    uint8_t  net_mode;          /* 0:自动 1:WiFi 2:4G */
    uint8_t  reserved[11];

    uint32_t crc32;

} AppParam_t;
```

### 11.2 参数保存策略

```text
上电读取参数 → 判断 magic + CRC → 无效则加载默认值
参数修改后延迟 3 秒再保存（避免频繁写入）
保存后重新读出校验
```

---

## 12. Git 版本管理

### 12.1 .gitignore

```gitignore
Objects/
Listings/
*.o
*.d
*.crf
*.axf
*.hex
*.bin
*.map
*.lst
*.lnp
*.htm
*.build_log.htm

*.uvguix.*
*.uvoptx
*.scvd
*.dbgconf

.vscode/ipch/
.vscode/.browse.c_cpp.db*

*.bak
*.tmp
*.orig
*.log
```

### 12.2 分支策略

```text
main：稳定演示版本
dev：日常开发版本
feature/bootloader
feature/ymodem
feature/modbus-master
feature/mqtt-4g
feature/ota
feature/freertos
```

### 12.3 提交规范

```bash
# 格式：[模块] 简洁描述
[boot] 初始化 Bootloader 主入口和魔数检测
[ymodem] 实现 YMODEM 控制字符解析及 CRC16 计算
[modbus] 完成 RS485 接收环形缓冲区
[ota] 下载分片写入 GD25Q128 并计算 CRC32
[mqtt] 完成 4G MQTT 连接和数据发布
```

### 12.4 每天结束固定流程

```bash
git status
git add .
git commit -m "说明今天完成的功能"
git push
```

---

## 13. V1 十天进度计划

### Day 01：建立工程和 Git 仓库

- [ ] 创建总目录
- [ ] 创建 Bootloader_Project
- [ ] 创建 App_Project
- [ ] 创建 Docs 和 Tools
- [ ] 添加 README.md
- [ ] 添加 .gitignore
- [ ] 初始化 Git
- [ ] 串口 printf 正常
- [ ] LED 闪烁正常

```bash
git commit -m "init: create modbus gateway project structure"
```

### Day 02：Bootloader 最小跳转 APP

- [ ] Bootloader IROM 设置 0x08000000
- [ ] APP IROM 设置 0x08008000
- [ ] APP 设置 VTOR
- [ ] Bootloader 实现 BootCheckAppValid
- [ ] Bootloader 实现 BootJumpToApp
- [ ] 上电先打印 BOOT，再打印 APP

```bash
git commit -m "feat: add bootloader jump to app"
```

### Day 03：AT24C02 参数保存

- [ ] 实现软件 IIC
- [ ] 实现 AT24C02 字节读写
- [ ] 实现 AT24C02 页写
- [ ] 定义 BootParam_t
- [ ] 实现 BootParamLoad / BootParamSave
- [ ] 断电参数保持测试

```bash
git commit -m "feat: add at24c02 boot parameter storage"
```

### Day 04：GD25Q128 驱动

- [ ] SPI 初始化
- [ ] 读取 JEDEC ID
- [ ] 扇区擦除
- [ ] 页写
- [ ] 连续读取
- [ ] 写入字符串并读回验证

```bash
git commit -m "feat: add external spi flash driver"
```

### Day 05：YMODEM 串口升级

- [ ] 串口接收进入环形缓冲区
- [ ] 实现 YMODEM 控制字符
- [ ] 实现首包解析
- [ ] 实现数据包 CRC16 校验
- [ ] 写入 GD25Q128 下载区
- [ ] 接收完成计算 CRC32

```bash
git commit -m "feat: add ymodem firmware receive"
```

### Day 06：Bootloader 搬运新固件

- [ ] 读取升级标志
- [ ] 校验 GD25Q128 固件
- [ ] 擦除 APP 区
- [ ] 分块写入内部 Flash
- [ ] 校验 APP CRC32
- [ ] 清除升级标志
- [ ] 跳转新 APP

```bash
git commit -m "feat: add firmware copy from external flash to app"
```

### Day 07：RS485 + Modbus 基础

- [ ] 实现 RS485 方向控制
- [ ] 实现 Modbus CRC16
- [ ] 实现 03 读保持寄存器
- [ ] 实现 05 写线圈
- [ ] 实现 06 写单寄存器
- [ ] 测试一个从机

```bash
git commit -m "feat: add modbus master basic functions"
```

### Day 08：数据中心 + 命令队列

- [ ] 建立 GatewayData_t
- [ ] 建立 Modbus 从机轮询表
- [ ] 建立 GatewayCmd_t
- [ ] 实现裸机命令队列
- [ ] HMI / MQTT 模拟写命令
- [ ] Modbus 统一执行命令

```bash
git commit -m "feat: add data center and command queue"
```

### Day 09：4G AT + MQTT

- [ ] 4G 串口接入环形缓冲区
- [ ] AT 命令发送和等待 OK
- [ ] MQTT 连接
- [ ] 发布数据
- [ ] 订阅控制 topic
- [ ] 解析 JSON 控制命令

```bash
git commit -m "feat: add 4g mqtt upload and downlink command"
```

### Day 10：整体验收和文档整理

- [ ] Bootloader 串口升级成功
- [ ] OTA 标志升级成功
- [ ] Modbus 采集成功
- [ ] MQTT 上传成功
- [ ] 云端下发控制成功
- [ ] README 更新
- [ ] Docs 更新
- [ ] 打 tag

```bash
git commit -m "docs: finish modbus gateway v1 record"
git tag -a v1.0.0 -m "Modbus Gateway V1 Bootloader OTA"
git push
git push origin v1.0.0
```

---

## 14. 裸机软件定时器

V1 裸机阶段没有 RTOS 的 vTaskDelay，需要用 SysTick 或定时器实现软件定时器：

```c
typedef struct
{
    uint32_t interval_ms;
    uint32_t last_tick;
    uint8_t enabled;
} SoftTimer_t;

uint8_t SoftTimerIsExpired(SoftTimer_t *timer)
{
    if(BspGetTick() - timer->last_tick >= timer->interval_ms)
    {
        timer->last_tick = BspGetTick();
        return 1;
    }
    return 0;
}
```

每个 Process 内部使用软件定时器控制执行周期，而不是靠 delay。

---

## 15. V2 FreeRTOS 迁移

### 15.1 任务划分

```text
AppModbusProcess   -> AppModbusTask      (5~10ms)
AppMqttProcess     -> AppMqttTask        (10ms~1s)
AppOtaProcess      -> AppOtaTask         (需要时运行)
AppSaveProcess     -> AppSaveTask        (100ms)
AppMonitorProcess  -> AppMonitorTask     (1s)
```

### 15.2 队列设计

| 队列 | 作用 |
|---|---|
| g_modbus_cmd_queue | Modbus 控制命令队列 |
| g_mqtt_rx_queue | MQTT 下发数据队列 |
| g_ota_queue | OTA 分片数据队列 |

### 15.3 互斥锁设计

| 资源 | 保护方式 |
|---|---|
| IIC 总线 | Mutex |
| SPI 总线 | Mutex |
| UART 发送 | Mutex |
| 系统参数 | Mutex |
| GatewayData_t | Mutex |

### 15.4 事件标志组设计

| 事件位 | 含义 |
|---|---|
| BIT_4G_CONNECTED | 4G 已连接 |
| BIT_MQTT_CONNECTED | MQTT 已连接 |
| BIT_OTA_START | OTA 开始 |
| BIT_OTA_DONE | OTA 完成 |

### 15.5 迁移原则

```text
V1 的 Process 函数体直接放进 Task 的 while(1) 中
Process 函数已经保证非阻塞，迁移时只需加 vTaskDelay
命令队列从裸机 RingQueue 替换为 xQueue
数据中心加 Mutex 保护
```

### 15.6 迁移示例

```c
/* V1 裸机 */
void AppModbusProcess(void) { /* ... */ }

/* V2 FreeRTOS */
void AppModbusTask(void *argument)
{
    while(1)
    {
        AppModbusProcess();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

裸机命令队列：

```c
GatewayCmdQueueWrite(&cmd);
GatewayCmdQueueRead(&cmd);
```

FreeRTOS 命令队列：

```c
xQueueSend(g_modbus_cmd_queue, &cmd, 0);
xQueueReceive(g_modbus_cmd_queue, &cmd, 0);
```

---

## 16. V3 RT-Thread 迁移

### 16.1 设备框架

```text
UART / SPI / IIC 使用 RT-Thread rt_device 接口
不再直接操作寄存器或标准库函数
```

### 16.2 组件化

```text
参数保存做成组件：at24c02_param
Modbus 做成组件：modbus_gateway
MQTT 做成组件：mqtt_client
OTA 做成组件：ota_manager
```

### 16.3 线程和通信

```text
使用 rt_thread 创建线程
使用 rt_mq（消息队列）传递命令
使用 rt_mutex 保护共享数据
使用 rt_event 管理连接状态
```

### 16.4 调试命令

```text
FinSH / MSH 添加调试命令：
    modbus poll 1       → 手动轮询 1 号从机
    modbus write 1 10 1234 → 写寄存器
    mqtt status         → 查看 MQTT 状态
    ota start           → 触发 OTA
    param show          → 显示参数
    param save          → 保存参数
```

---

## 17. 简历描述建议

### 17.1 项目名称

> 基于 GD32F303 的 Modbus RTU 工业数据采集网关

### 17.2 项目描述

> 本项目基于 GD32F303 主控实现了一套 Modbus RTU 工业数据采集网关，支持 RS485 多从机轮询采集、4G MQTT 上云、云端下发命令控制从机、Bootloader 串口/OTA 双通道固件升级。项目采用分层模块化设计（Driver / Component / Mid / App），V1 使用裸机任务调度框架实现，所有模块封装为 Init + Process 形式，后续平滑迁移到 FreeRTOS 多任务架构。系统支持 AT24C02 参数掉电保存、GD25Q128 固件缓存和备份、CRC32 完整性校验、升级失败自动回退等工业级可靠性设计。

### 17.3 技术要点

- GD32F303 标准库开发
- UART + RingBuffer 串口接收
- 软件 IIC 驱动 AT24C02
- SPI 驱动 GD25Q128
- RS485 方向控制
- Modbus RTU 主站协议（03/05/06/10 功能码）
- 多从机轮询和命令队列
- AT 指令框架驱动 4G 模块
- MQTT 数据上传和命令下发
- Bootloader + YMODEM + CRC32 + OTA
- 裸机任务调度 → FreeRTOS 迁移
- 队列、互斥锁、事件标志组

---

## 18. 调试记录模板

```text
日期：
开发内容：
当前分支：
提交记录：

问题现象：
可能原因：
排查过程：
最终解决：
后续注意：

今日完成：
- [ ]
- [ ]
- [ ]

明日计划：
- [ ]
- [ ]
- [ ]
```

---

## 19. AT24C02 使用注意事项

- AT24C02 总容量 256 字节，页大小 8 字节
- 写入后需要等待内部写周期（约 5ms），或用 ACK 轮询
- 页写不能跨页，需要分页处理
- BootParam_t 约 48 字节，需要 6 页
- 建议主参数和备份参数各占一页对齐区域
- 地址分配：主参数 0x00~0x2F，备份参数 0x80~0xAF

---

## 20. GD25Q128 / W25Q64 使用注意事项

- GD25Q128 JEDEC ID：C8 40 18
- W25Q64 JEDEC ID：EF 40 17
- 4KB 扇区擦除，256 字节页编程
- 写入前必须擦除（擦除后数据为 0xFF）
- 页编程不能跨页，需要分页处理
- 写入后需要等待 Busy 位清除
- 固件下载区 512KB 足够存放 480KB APP

---

## 21. 最重要注意事项

1. **APP 不能自己覆盖内部 Flash 当前运行区。**
2. **APP OTA 只写 GD25Q128，真正升级由 Bootloader 完成。**
3. **APP 必须设置 `SCB->VTOR = 0x08008000`。**
4. **AT24C02 保存小参数，GD25Q128 保存大固件。**
5. **MQTT / HMI / 上位机不直接操作 RS485，只写命令队列。**
6. **Modbus 总线只允许 AppModbusProcess 操作。**
7. **每个 App 模块保持 Init + Process，方便 V2 迁移。**
8. **每完成一天就 Git 提交一次。**
