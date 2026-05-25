# 基于 GD32/STM32 的串口 LoRa 无线数据采集网关项目方案

## 1. 项目定位

本项目面向嵌入式实习作品集，基于现有的 **3 个串口 LoRa 模块**、**GD32 开发板**、**STM32 开发板**、**CH 板**、**CW 板**，设计一套多节点无线数据采集系统。

系统采用 **1 个主站网关 + 2 个从站节点** 的结构。从站节点周期性采集传感器数据或模拟数据，通过串口 LoRa 模块无线发送到主站网关。主站完成 LoRa 数据接收、协议解析、数据校验、ACK 应答、串口日志输出，后续可扩展 OLED 显示、4G MQTT 上传、Bootloader OTA 和 Modbus 网关功能。

融合后的项目主线是：

```text
串口 LoRa 模块调通
        |
        v
单个从站无线发送传感器数据
        |
        v
主站网关解析自定义协议并返回 ACK
        |
        v
扩展为 1 主 2 从多节点采集
        |
        v
增加 OLED / 上位机 / 4G MQTT / Bootloader OTA
```

也就是说，这个项目不是只做一个 LoRa 串口收发 Demo，而是做成一个完整的 **远程环境数据采集网关**。第一版先保证能演示，后续逐步增强成简历上的综合项目。

该项目重点体现：

1. 串口通信与中断接收能力
2. 串口 LoRa 模块调试与应用能力
3. 自定义无线通信协议设计能力
4. 多节点数据采集与主从通信能力
5. ACK 应答、超时重发、异常处理能力
6. 有限状态机设计能力
7. 嵌入式软件分层架构设计能力
8. 后续扩展 4G、MQTT、OTA、Modbus 的工程能力

## 2. 项目名称

推荐名称：

```text
基于 GD32F303 的 LoRa 无线传感数据采集网关
```

GitHub 或工程目录名称可以使用：

```text
LoRa_Wireless_Gateway
```

如果后续加入 4G MQTT，可以升级为：

```text
基于 GD32F303 的多节点 LoRa 无线采集与 4G 网关系统
```

## 3. 现有硬件资源

当前已知硬件资源如下：

| 硬件 | 数量 | 推荐用途 |
|---|---:|---|
| 串口 LoRa 模块 | 3 | 1 个主站，2 个从站 |
| GD32 开发板 | 1 | 主站网关 |
| STM32 开发板 | 1 | 从站节点 1 |
| CH 板 | 1 | 从站节点 2 或备选测试板 |
| CW 板 | 1 | 从站节点 2 或备选测试板 |
| USB-TTL 模块 | 1 个或多个 | LoRa 模块调试、串口日志 |
| 传感器 | 可选 | 温湿度、光照、电压、空气质量 |
| OLED | 可选 | 主站数据显示 |

第一版不强依赖真实传感器，可以先使用模拟数据完成通信闭环。

## 3.1 传感器与采集数据规划

为了让项目更适合实习展示，建议从站节点不要只发送字符串，而是包装成“远程环境数据采集节点”。

推荐数据项：

| 数据项 | 推荐器件 | 接口 | 第一版替代方案 |
|---|---|---|---|
| 温度 | SHT30 / DHT11 / DS18B20 | I2C / 单总线 | 模拟固定值 |
| 湿度 | SHT30 / DHT11 | I2C / 单总线 | 模拟固定值 |
| 光照 | BH1750 | I2C | 模拟固定值 |
| 电池电压 | 电阻分压 + ADC | ADC | 模拟固定值 |
| 空气质量 | MQ 系列 / 其他模块 | ADC / UART | 模拟固定值 |
| 开关状态 | 按键 / GPIO | GPIO | 固定状态值 |

第一版推荐：

```text
节点 1：温度 + 湿度
节点 2：空气质量 + 状态量
```

没有传感器时，先用模拟数据：

```text
Node 1:
Temp = 25.6
Humi = 58.3

Node 2:
AirQuality = 120
Status = 1
```

等通信链路跑通后，再把模拟数据替换为真实传感器读取函数。

## 4. 硬件角色分配

推荐分配方式：

| 角色 | 推荐板子 | LoRa 模块 | 节点地址 | 功能 |
|---|---|---|---:|---|
| 主站网关 | GD32F303 | LoRa 模块 1 | `0x00` | 接收数据、解析协议、返回 ACK、打印日志 |
| 从站节点 1 | STM32 | LoRa 模块 2 | `0x01` | 周期上报温湿度或模拟数据 |
| 从站节点 2 | CH 板 / CW 板 | LoRa 模块 3 | `0x02` | 周期上报空气质量、状态量或模拟数据 |

开发建议：

1. 先完成 **1 主 1 从**，确认串口和 LoRa 通信稳定。
2. 再扩展到 **1 主 2 从**，验证多节点接入。
3. CH 板和 CW 板具体型号确认后，再决定哪个做正式从站。

## 5. 系统整体结构

```text
              从站节点 1
           STM32 / CH / CW
        传感器数据或模拟数据
                  |
                  | UART
                  v
             串口 LoRa 模块
                  |
                  | LoRa 无线
                  v
       +--------------------------+
       |                          |
       |      主站网关 GD32       |
       |      串口 LoRa 模块      |
       |                          |
       +--------------------------+
                  ^
                  | LoRa 无线
                  |
             串口 LoRa 模块
                  ^
                  | UART
              从站节点 2
           CH / CW / STM32
        传感器数据或模拟数据


主站网关后续扩展：

GD32 主站
  |
  |-- 调试串口日志
  |-- OLED 数据显示
  |-- 4G MQTT 云平台上传
  |-- Bootloader OTA 升级
  |-- Modbus RTU 网关扩展
```

## 6. 第一版功能范围

第一版目标是完成最小可展示闭环：

```text
从站采集或生成数据
        |
        v
按自定义协议打包
        |
        v
串口发送到 LoRa 模块
        |
        v
LoRa 无线传输
        |
        v
主站接收并解析
        |
        v
主站返回 ACK
        |
        v
从站收到 ACK，等待下一次发送
```

V1 功能清单：

1. 3 个 LoRa 模块参数配置一致
2. GD32 主站串口接收 LoRa 数据
3. STM32 / CH / CW 从站周期发送数据
4. 自定义协议帧打包与解析
5. 校验和检查
6. 主站返回 ACK 应答
7. 从站 ACK 超时重发
8. 主站串口日志输出
9. 多节点地址区分
10. LED 状态指示

## 7. 串口分配建议

### 7.1 主站 GD32

| 外设 | 用途 |
|---|---|
| USART0 | 调试串口，连接电脑，输出 printf 日志 |
| USART2 | LoRa 串口，连接串口 LoRa 模块 |
| GPIO | LED 状态指示 |
| I2C | 后续连接 OLED |

连接方式：

```text
GD32 USART0_TX  -> USB-TTL_RX
GD32 USART0_RX  -> USB-TTL_TX
GD32 USART2_TX  -> LoRa_RX
GD32 USART2_RX  -> LoRa_TX
GD32 GND        -> LoRa_GND
GD32 VCC        -> LoRa_VCC
```

### 7.2 从站 STM32 / CH / CW

推荐从站至少使用 1 路串口连接 LoRa 模块。

如果板子有 2 路串口：

| 外设 | 用途 |
|---|---|
| USART1 | 调试串口 |
| USART2 | LoRa 串口 |

如果板子只有 1 路串口：

| 外设 | 用途 |
|---|---|
| USART1 | 连接 LoRa 模块 |

单串口情况下，第一版可以先不做从站日志，通过主站日志观察通信结果。

## 8. LoRa 模块调试要求

项目开始前，需要先确认 3 个 LoRa 模块能够互通。

需要确认的参数：

1. 模块型号
2. 供电电压
3. 默认串口波特率
4. 是否支持 AT 指令
5. 是否支持透传模式
6. 是否支持地址、信道、空中速率配置
7. 频段是否一致
8. 发送功率是否可配置

推荐统一配置：

| 参数 | 推荐值 |
|---|---|
| 串口波特率 | `9600` 或模块默认值 |
| 数据位 | `8` |
| 停止位 | `1` |
| 校验位 | `None` |
| 工作模式 | 透传模式 |
| 信道 | 三个模块保持一致 |
| 空中速率 | 三个模块保持一致 |
| 发送功率 | 默认或最大 |

LoRa 模块第一阶段验收标准：

```text
A 模块通过串口助手发送 hello
B 模块可以收到 hello
C 模块也可以收到 hello
```

### 8.1 LoRa 模块驱动边界

串口 LoRa 模块通常已经封装了底层 LoRa 射频细节，MCU 侧重点放在 UART 通信和模块参数配置上。

第一版只做透传收发：

```c
void LoraInit(void);
void LoraSendBytes(uint8_t *data, uint16_t len);
uint16_t LoraReadBytes(uint8_t *buf, uint16_t max_len);
```

如果模块支持 AT 指令，再增加配置接口：

```c
uint8_t LoraSendCmd(const char *cmd, const char *expect, uint32_t timeout_ms);
uint8_t LoraSetAddress(uint16_t addr);
uint8_t LoraSetChannel(uint8_t channel);
uint8_t LoraSetBaudrate(uint32_t baudrate);
uint8_t LoraSetPower(uint8_t power);
uint8_t LoraSetMode(uint8_t mode);
```

AT 指令配置不是第一天必须完成的内容。推荐先用串口助手把 3 个模块参数配成一致，再让 MCU 做透传收发；等主链路稳定后，再把 AT 配置写进 `component_lora`。

## 9. 通信协议设计

第一版不要直接发送普通字符串，而是设计一个轻量级数据帧协议，体现工程完整度。

### 9.1 数据帧格式

```text
帧头      目标地址   源地址    功能码    数据长度    数据区      校验
0xAA55    dst       src       cmd       len        payload    checksum
```

字节格式：

```text
AA 55 | DST | SRC | CMD | LEN | PAYLOAD | CHECK
```

### 9.2 地址规划

```c
#define LORA_ADDR_GATEWAY       0x00
#define LORA_ADDR_NODE_1        0x01
#define LORA_ADDR_NODE_2        0x02
#define LORA_ADDR_BROADCAST     0xFF
```

地址含义：

| 地址 | 含义 |
|---:|---|
| `0x00` | 主站网关 |
| `0x01` | 从站节点 1 |
| `0x02` | 从站节点 2 |
| `0xFF` | 广播地址 |

### 9.3 功能码规划

```c
#define LORA_CMD_HEARTBEAT          0x00
#define LORA_CMD_SENSOR_REPORT      0x01
#define LORA_CMD_PARAM_SET          0x02
#define LORA_CMD_PARAM_READ         0x03
#define LORA_CMD_ACK                0x80
#define LORA_CMD_ERROR              0xFF
```

功能码含义：

| 功能码 | 含义 |
|---:|---|
| `0x00` | 心跳包 |
| `0x01` | 传感器数据上报 |
| `0x02` | 参数设置 |
| `0x03` | 参数读取 |
| `0x80` | ACK 应答 |
| `0xFF` | 错误响应 |

### 9.4 校验方式

第一版可以先用简单累加和：

```text
CHECK = DST + SRC + CMD + LEN + PAYLOAD 所有字节累加后的低 8 位
```

后续升级为 CRC16：

```text
CRC16-Modbus
```

建议路线：

1. V1 使用 1 字节 checksum，便于快速调通。
2. V2 改为 CRC16，提高可靠性和面试表达质量。

## 10. 数据帧示例

### 10.1 节点 1 温湿度上报

节点 1 向主站上报温湿度：

```text
AA 55 00 01 01 04 00 FA 02 47 CHECK
```

解释：

| 字段 | 值 | 含义 |
|---|---|---|
| 帧头 | `AA 55` | 固定帧头 |
| DST | `00` | 目标地址，主站 |
| SRC | `01` | 源地址，节点 1 |
| CMD | `01` | 传感器上报 |
| LEN | `04` | 数据区 4 字节 |
| DATA0-1 | `00 FA` | 温度 25.0 摄氏度，实际值放大 10 倍 |
| DATA2-3 | `02 47` | 湿度 58.3%，实际值放大 10 倍 |
| CHECK | `xx` | 校验值 |

数据换算：

```text
温度 25.0 -> 250 -> 0x00FA
湿度 58.3 -> 583 -> 0x0247
```

### 10.2 节点 2 空气质量上报

节点 2 向主站上报空气质量和状态：

```text
AA 55 00 02 01 03 00 78 01 CHECK
```

解释：

| 字段 | 值 | 含义 |
|---|---|---|
| 帧头 | `AA 55` | 固定帧头 |
| DST | `00` | 目标地址，主站 |
| SRC | `02` | 源地址，节点 2 |
| CMD | `01` | 传感器上报 |
| LEN | `03` | 数据区 3 字节 |
| DATA0-1 | `00 78` | 空气质量数值 120 |
| DATA2 | `01` | 状态正常 |
| CHECK | `xx` | 校验值 |

### 10.3 主站 ACK 应答

主站收到节点 1 数据后返回 ACK：

```text
AA 55 01 00 80 01 00 CHECK
```

解释：

| 字段 | 值 | 含义 |
|---|---|---|
| DST | `01` | 目标地址，节点 1 |
| SRC | `00` | 源地址，主站 |
| CMD | `80` | ACK |
| LEN | `01` | 数据区 1 字节 |
| DATA0 | `00` | 0 表示成功 |

## 11. 软件分层结构

推荐工程目录结构：

```text
User
├── app
│   ├── app_gateway.c
│   ├── app_gateway.h
│   ├── app_node.c
│   └── app_node.h
│
├── component
│   ├── component_lora.c
│   ├── component_lora.h
│   ├── component_protocol.c
│   └── component_protocol.h
│
├── driver
│   ├── driver_uart.c
│   ├── driver_uart.h
│   ├── driver_led.c
│   ├── driver_led.h
│   ├── driver_key.c
│   └── driver_key.h
│
├── bsp
│   ├── bsp_tick.c
│   └── bsp_tick.h
│
└── config
    └── app_config.h
```

各层职责：

| 层级 | 负责内容 |
|---|---|
| `driver` | UART、LED、KEY 等底层外设驱动 |
| `bsp` | 系统节拍、板级初始化 |
| `component` | LoRa 模块、协议解析、通用组件 |
| `app` | 主站业务逻辑、从站业务逻辑 |
| `config` | 节点地址、串口选择、功能开关配置 |

## 12. 核心模块设计

### 12.1 driver_uart

负责底层串口驱动：

1. 串口初始化
2. 串口发送字节
3. 串口发送数组
4. 串口发送字符串
5. 串口中断接收
6. RingBuffer 接收缓存
7. 接收超时判断

### 12.2 component_lora

负责 LoRa 模块抽象：

1. LoRa 模块初始化
2. LoRa 数据发送
3. LoRa 数据接收
4. AT 指令发送
5. 模块参数配置
6. 模块通信状态检测

第一版可以只实现透传收发：

```c
void LoraInit(void);
void LoraSendBytes(uint8_t *data, uint16_t len);
uint16_t LoraReadBytes(uint8_t *buf, uint16_t max_len);
```

### 12.3 component_protocol

负责协议打包与解析：

1. 计算 checksum
2. 打包数据帧
3. 解析数据帧
4. 校验帧头
5. 校验长度
6. 校验目标地址
7. 校验 checksum

推荐接口：

```c
uint8_t ProtocolChecksum(uint8_t *data, uint16_t len);
uint16_t ProtocolPackFrame(uint8_t dst,
                           uint8_t src,
                           uint8_t cmd,
                           uint8_t *payload,
                           uint8_t payload_len,
                           uint8_t *out_buf);

int ProtocolParseFrame(uint8_t *buf,
                       uint16_t len,
                       LoraFrame_t *frame);
```

### 12.4 app_gateway

主站业务：

1. 接收 LoRa 数据
2. 调用协议解析
3. 根据源地址区分节点
4. 根据功能码处理数据
5. 打印节点数据
6. 返回 ACK
7. 统计接收成功、失败、校验错误次数

### 12.5 app_node

从站业务：

1. 周期采集传感器数据或生成模拟数据
2. 调用协议模块打包
3. 发送 LoRa 数据帧
4. 等待主站 ACK
5. 超时重发
6. 失败后记录错误计数
7. 进入下一次采集周期

## 13. 主站状态机

```c
typedef enum
{
    GATEWAY_STATE_INIT = 0,
    GATEWAY_STATE_IDLE,
    GATEWAY_STATE_RECEIVE,
    GATEWAY_STATE_PARSE,
    GATEWAY_STATE_SEND_ACK,
    GATEWAY_STATE_ERROR
} GatewayState_t;
```

主站流程：

```text
INIT
  |
  v
IDLE
  |
  | 收到 LoRa 数据
  v
RECEIVE
  |
  v
PARSE
  |
  | 校验正确
  v
SEND_ACK
  |
  v
IDLE
```

## 14. 从站状态机

```c
typedef enum
{
    NODE_STATE_INIT = 0,
    NODE_STATE_COLLECT,
    NODE_STATE_PACK,
    NODE_STATE_SEND,
    NODE_STATE_WAIT_ACK,
    NODE_STATE_RETRY,
    NODE_STATE_SLEEP
} NodeState_t;
```

从站流程：

```text
INIT
  |
  v
COLLECT
  |
  v
PACK
  |
  v
SEND
  |
  v
WAIT_ACK
  |----------------------|
  | ACK OK               | Timeout
  v                      v
SLEEP                  RETRY
  |                      |
  |                      v
  |                    SEND
  v
下一周期
```

建议参数：

| 参数 | 推荐值 |
|---|---:|
| ACK 等待时间 | `500 ms` |
| 最大重发次数 | `3` |
| 节点 1 发送周期 | `2000 ms` |
| 节点 2 发送周期 | `3000 ms` |

## 15. 多节点冲突处理

由于串口 LoRa 模块第一版通常采用透传模式，多节点同时发送可能会造成无线冲突。

第一版采用错开发送周期：

```text
节点 1：每 2 秒发送一次
节点 2：每 3 秒发送一次
```

后续可升级：

1. 主站轮询节点
2. 节点随机退避
3. 节点发送前检测空闲
4. 加入帧序号和重复包过滤

## 16. 开发计划

### 第 1 阶段：LoRa 模块独立测试

目标：确认 3 个 LoRa 模块都能正常通信。

任务：

1. USB-TTL 分别连接 3 个 LoRa 模块
2. 确认默认波特率和工作模式
3. 配置相同信道、频段、空中速率
4. 两两互发字符串
5. 保存测试截图和模块参数

完成标准：

```text
A 模块发送 hello
B 模块可以收到 hello
C 模块可以收到 hello
```

### 第 2 阶段：GD32 主站串口框架

目标：GD32 可以通过 LoRa 串口接收数据，并通过调试串口打印。

任务：

1. 建立 GD32 主站工程
2. 完成 SysTick 1ms
3. 完成 LED
4. 完成 USART0 printf
5. 完成 USART2 LoRa 串口
6. LoRa 收到数据后打印到 USART0

完成标准：

```text
LoRa RX: hello
```

### 第 3 阶段：从站周期发送

目标：从站可以周期发送模拟数据。

任务：

1. 建立 STM32 / CH / CW 从站工程
2. 初始化 LoRa 串口
3. 每 1 秒发送一次模拟数据
4. 主站接收并打印

完成标准：

```text
Node1 Send Data: Temp=25.6 Humi=58.3
```

### 第 4 阶段：自定义协议打包和解析

目标：从普通字符串升级为二进制协议帧。

任务：

1. 编写 `component_protocol.c`
2. 实现 checksum
3. 实现协议打包
4. 实现协议解析
5. 从站发送 `AA 55` 协议帧
6. 主站解析地址、命令、数据、校验

完成标准：

```text
Frame OK
Src Addr: 0x01
Cmd: 0x01
Temp: 25.6
Humi: 58.3
```

### 第 5 阶段：ACK 应答和超时重发

目标：实现完整通信闭环。

任务：

1. 主站收到正确数据后返回 ACK
2. 从站发送数据后等待 ACK
3. ACK 超时时间设置为 500 ms
4. 超时未收到 ACK 自动重发
5. 最大重发 3 次

完成标准：

```text
Send Sensor Data
Wait ACK
ACK OK
```

异常情况：

```text
Wait ACK Timeout
Retry 1
Retry 2
Retry 3
Send Failed
```

### 第 6 阶段：1 主 2 从多节点通信

目标：3 个 LoRa 模块全部用上。

任务：

1. 主站地址设置为 `0x00`
2. 从站 1 地址设置为 `0x01`
3. 从站 2 地址设置为 `0x02`
4. 两个从站周期性发送数据
5. 主站根据源地址区分节点
6. 错开发送周期，降低冲突概率

完成标准：

```text
[Node 1] Temp=25.6 Humi=58.3
[Node 2] Air=120 Status=OK
```

### 第 7 阶段：工程整理和作品化

目标：把能跑的代码整理成可以展示的实习项目。

任务：

1. 主站状态机整理
2. 从站状态机整理
3. 错误帧统计
4. 丢包统计
5. LED 状态指示
6. README 编写
7. 协议文档编写
8. 测试截图整理
9. GitHub 仓库整理

完成标准：

1. 代码结构清晰
2. README 能说明项目价值
3. 协议文档完整
4. 有串口日志截图
5. 有实物连接图
6. 能现场演示 1 主 2 从通信

## 17. 版本规划

### V1：LoRa 无线采集基础版

1. 1 主 2 从
2. 串口 LoRa 透传
3. 自定义协议
4. ACK 应答
5. 超时重发
6. 串口日志

### V2：LoRa + OLED 显示版

1. 主站增加 OLED
2. 显示节点在线状态
3. 显示节点传感器数据
4. 显示通信成功和失败次数

### V3：LoRa + 4G MQTT 网关版

1. 主站增加 4G 模块
2. LoRa 收集节点数据
3. 通过 MQTT 上传云平台
4. 云端查看节点状态

### V4：LoRa + Bootloader OTA 版

1. 结合现有 Bootloader 工程
2. 主站支持本地固件升级
3. 后续支持 4G 下载固件
4. 固件缓存到外部 Flash
5. Bootloader 搬运并升级 APP

### V5：LoRa + Modbus 网关扩展版

1. 支持 Modbus RTU 设备采集
2. 支持 LoRa 节点数据采集
3. 主站统一管理多类数据
4. 后续通过 4G 上传云平台

### V6：LoRa + PC 上位机可视化版

1. PC 端通过串口连接主站
2. 实时显示节点温湿度、空气质量、电压等数据
3. 保存历史数据到 CSV
4. 绘制温度、湿度变化曲线
5. 支持下发简单控制命令，例如修改上报周期、控制节点 LED

上位机实现方案：

| 方案 | 技术 | 适合情况 |
|---|---|---|
| 简单版 | 串口助手 + 日志格式化 | 最快出效果 |
| Python 版 | PyQt / Tkinter + pyserial | 适合做作品展示 |
| 曲线版 | pyserial + matplotlib | 适合展示数据趋势 |
| Web 版 | Node.js / Electron / Web Serial | 后续扩展空间大 |

第一版不强制做上位机，但如果时间允许，建议至少做一个 Python 串口数据显示界面。它对实习面试很直观，面试官一眼能看出系统在跑。

## 17.1 最小可交付版本

为了避免项目做散，建议先锁定一个最小可交付版本：

```text
1 个 GD32 主站
2 个从站节点
3 个串口 LoRa 模块
自定义 AA55 协议
节点数据上报
主站 ACK
从站超时重发
串口日志展示
```

这个版本完成后，就已经可以作为实习项目写进简历。

最小版本不要求：

1. 不要求一开始就接真实传感器
2. 不要求一开始就做 4G
3. 不要求一开始就做云平台
4. 不要求一开始就做 OTA
5. 不要求一开始就做复杂上位机

先把通信闭环做稳，再逐步加功能。

## 18. 调试日志示例

### 18.1 主站启动日志

```text
LoRa Gateway Start
Board: GD32F303
Gateway Addr: 0x00
LoRa UART Init OK
Protocol Init OK
```

### 18.2 主站接收节点 1

```text
[RX OK]
Src Addr : 0x01
Cmd      : SENSOR_REPORT
Temp     : 25.6 C
Humi     : 58.3 %
Check    : OK
ACK      : SEND OK
```

### 18.3 主站接收节点 2

```text
[RX OK]
Src Addr : 0x02
Cmd      : SENSOR_REPORT
Air      : 120
Status   : OK
Check    : OK
ACK      : SEND OK
```

### 18.4 从站发送日志

```text
Node Start
Node Addr: 0x01
Collect Sensor Data
Pack Frame OK
Send Frame OK
Wait ACK
ACK OK
```

## 19. 测试记录模板

| 日期 | 测试内容 | 测试结果 | 问题记录 |
|---|---|---|---|
|  | LoRa 模块 A -> B 字符串互发 |  |  |
|  | LoRa 模块 A -> C 字符串互发 |  |  |
|  | GD32 主站接收 LoRa 数据 |  |  |
|  | 从站 1 协议帧发送 |  |  |
|  | 主站 ACK 应答 |  |  |
|  | 从站 ACK 超时重发 |  |  |
|  | 1 主 2 从同时运行 |  |  |

## 20. README 展示内容建议

GitHub README 建议包含：

1. 项目简介
2. 项目硬件
3. 系统架构图
4. 功能特性
5. 通信协议说明
6. 工程目录结构
7. 编译和下载方法
8. 测试日志截图
9. 实物连接图
10. 后续升级计划

## 21. 简历描述

### 21.1 项目标题

```text
基于 GD32F303 的 LoRa 无线传感数据采集网关
```

### 21.2 项目描述

基于 GD32F303、STM32 和串口 LoRa 模块，设计并实现一套多节点无线数据采集系统。系统采用 1 个主站网关和 2 个从站节点的结构，从站周期性采集温湿度、空气质量等环境数据，并通过 LoRa 无线发送至主站。主站完成数据接收、协议解析、校验判断、ACK 应答和串口日志输出。项目采用分层软件架构，将 UART 驱动、LoRa 模块控制、通信协议解析和业务逻辑进行解耦，便于后续扩展 OLED 显示、4G MQTT 上传和 Bootloader OTA 升级功能。

### 21.3 简历要点

1. 基于 GD32F303 和 STM32 搭建 1 主 2 从 LoRa 无线采集系统，实现多节点数据上报和主站集中接收。
2. 编写串口 LoRa 模块驱动，支持 UART 透传收发、接收缓存、超时判断和模块参数配置。
3. 设计自定义应用层通信协议，包含帧头、目标地址、源地址、功能码、数据长度、数据区和 checksum 校验。
4. 实现 ACK 应答和超时重发机制，提高无线通信可靠性。
5. 使用有限状态机管理从站采集、组帧、发送、等待 ACK、重发等流程。
6. 采用分层架构设计，将底层驱动、LoRa 组件、协议解析和应用逻辑解耦，便于后续维护和扩展。

### 21.4 技术关键词

```text
GD32F303、STM32、USART、LoRa、无线通信、自定义协议、
RingBuffer、状态机、ACK 应答、超时重发、多节点采集、
嵌入式分层架构、OLED、MQTT、Bootloader、Modbus
```

## 22. 当前最优先任务

现在不要急着写完整代码，先完成最小硬件闭环。

第一步目标：

```text
3 个 LoRa 模块全部测试通过
```

具体任务：

1. 确认 LoRa 模块型号
2. 确认供电电压
3. 确认串口默认波特率
4. 确认是否支持 AT 指令
5. 使用 USB-TTL 测试两个 LoRa 模块互发
6. 将第三个 LoRa 模块加入测试
7. 记录模块参数、串口助手截图、接线方式

第一阶段完成标准：

```text
A 模块发 hello
B 模块能收到 hello
C 模块也能收到 hello
```

完成这个闭环后，再开始搭 GD32 主站工程。
