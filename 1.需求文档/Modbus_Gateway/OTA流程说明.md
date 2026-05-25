# Modbus 网关 OTA 升级流程说明

## 1. 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                          服务器 (ThingsBoard)                    │
└───────────────────────────────┬─────────────────────────────────┘
                                │ MQTT
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                         4G 模块 (EC200U)                         │
└───────────────────────────────┬─────────────────────────────────┘
                                │ UART
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      GD32F303ZET6 MCU                            │
│                                                                  │
│  ┌──────────────────┐          ┌──────────────────┐             │
│  │   Bootloader     │          │      App         │             │
│  │  0x08000000      │          │   0x08008000     │             │
│  │   (32KB)         │          │                  │             │
│  └──────────────────┘          └──────────────────┘             │
│                                                                  │
│  ┌──────────────────┐          ┌──────────────────┐             │
│  │  GD25Q128        │          │    AT24C02       │             │
│  │  (16MB SPI Flash)│          │  (256B I2C EEPROM)│             │
│  └──────────────────┘          └──────────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

## 2. 存储规划

### 2.1 内部 Flash (GD32F303ZET6)

| 区域 | 起始地址 | 大小 | 用途 |
|------|----------|------|------|
| Bootloader | 0x08000000 | 32KB | 启动程序 |
| APP | 0x08008000 | 剩余空间 | 应用程序 |

### 2.2 外部 Flash (GD25Q128)

| 区域 | 起始地址 | 大小 | 用途 |
|------|----------|------|------|
| 下载区 | 0x000000 | 512KB | OTA 固件临时存储 |
| 固件信息头 | 0x0F0000 | - | 固件元数据 |

### 2.3 AT24C02 EEPROM

| 地址 | 长度 | 用途 |
|------|------|------|
| 0x00 | 4 字节 | OTA 升级标志 (0x5A5A5A5A = 需要升级) |
| 0x04 | 32 字节 | 版本字符串 (如 "1.0.0") |

## 3. MQTT Topic 定义

| Topic | 方向 | 用途 |
|-------|------|------|
| `v1/devices/me/attributes` | 服务器 → 设备 | OTA 通知 (下发 fw_title/fw_version/fw_size/fw_checksum) |
| `v2/fw/request/{id}/chunk/{id}` | 设备 → 服务器 | 请求固件分包 |
| `v2/fw/response/{id}/chunk/{id},{bytes},{data}` | 服务器 → 设备 | 固件分包响应 |
| `v1/devices/me/telemetry` | 设备 → 服务器 | 上报升级状态 |

## 4. OTA 完整流程

```
服务器                     App                    GD25Q128           AT24C02         Bootloader
  │                         │                       │                  │                │
  ├─ OTA通知 ──────────────►│                       │                  │                │
  │  (attributes+fw_title)  │                       │                  │                │
  │                         │                       │                  │                │
  │◄─ 请求分包0 ────────────┤                       │                  │                │
  │   (v2/fw/request/...)   │                       │                  │                │
  │                         │                       │                  │                │
  ├─ 分包0数据 ─────────────►│                       │                  │                │
  │  (v2/fw/response/...)   ├─ 写入 ───────────────►│                  │                │
  │                         │                       │                  │                │
  │◄─ 请求分包1 ────────────┤                       │                  │                │
  ├─ 分包1数据 ─────────────►│                       │                  │                │
  │                         ├─ 写入 ───────────────►│                  │                │
  │         ...             │         ...           │                  │                │
  │                         │                       │                  │                │
  │                         ├─ CRC32校验通过         │                  │                │
  │                         ├─ 写固件信息头 ────────►│                  │                │
  │                         ├─ 写升级标志 ─────────────────────────────►│                │
  │                         ├─ 写版本信息 ─────────────────────────────►│                │
  │                         │                       │                  │                │
  │◄─ 上报UPDATED ──────────┤                       │                  │                │
  │                         ├─ 重启 ──────────────────────────────────────────────────►│
  │                         │                       │                  │                │
  │                         │                       │                  │  读标志=0x5A    │
  │                         │                       │                  │◄─ 清除标志 ─────┤
  │                         │                       │                  │                │
  │                         │                       │◄─ 读固件信息头 ──┤                │
  │                         │                       │◄─ CRC32校验 ─────┤                │
  │                         │                       │◄─ 搬运到内部Flash─┤                │
  │                         │                       │                  │                │
  │                         │                       │                  │    跳转APP ────►│
  │                         │                       │                  │                │
  │                         │◄────────────────────── 运行新固件 ────────────────────────┤
```

## 5. App 侧流程

### 5.1 初始化

```
AppTaskInit()
    ├─ DrvEepromInit()         // 初始化 AT24C02 I2C
    ├─ ComponentOtaInit()      // 初始化 OTA 模块
    ├─ App4GInit()             // 初始化 4G/MQTT
    └─ ...
```

### 5.2 主循环

```
主循环 AppTaskRun()
    │
    ├─ App4GProcess()              // 4G 通信处理
    │   │
    │   ├─ 4G 初始化未完成?
    │   │   └─ 继续初始化 AT 命令序列
    │   │
    │   └─ 4G 初始化完成 → App4GProcessIncoming() 处理串口数据
    │       │
    │       ├─ 收到 attributes topic + fw_title?
    │       │   ├─ ComponentOtaParseNotify() 解析参数
    │       │   └─ ComponentOtaTrigger() 设置触发标志
    │       │
    │       ├─ 收到 v2/fw/response/... 二进制分包?
    │       │   └─ ComponentOtaFeedChunk() 存入缓冲区
    │       │
    │       └─ 收到 RPC JSON?
    │           └─ App4GPullData() 解析命令
    │
    ├─ TaskOtaProcess()            // OTA 处理
    │   └─ ComponentOtaProcess()
    │       └─ ota_trigger == 1? → OtaDownloadFirmware()
    │
    ├─ TaskModbusProcess()         // Modbus 通信
    ├─ TaskHTRead()                // 温湿度采集
    ├─ TaskADCRead()               // ADC 采集
    └─ ...
```

### 5.3 OTA 下载流程 (OtaDownloadFirmware)

```
OtaDownloadFirmware()
    │
    ├─ 1. 检查 fw_size 有效性 (0 < fw_size <= 512KB)
    │
    ├─ 2. 计算总分包数 = ceil(fw_size / 256)
    │
    ├─ 3. FlashFwInit() 初始化 GD25Q128
    │
    ├─ 4. FlashFwEraseDownloadArea() 擦除下载区 (0x000000)
    │
    ├─ 5. 逐包下载循环:
    │   │
    │   ├─ OtaSendChunkRequest()
    │   │   发送到: v2/fw/request/{id}/chunk/{id}
    │   │   payload: 请求字节数 (256 或剩余字节数)
    │   │
    │   ├─ 等待 recv_flag (10秒超时)
    │   │
    │   ├─ FlashFwWriteDownload()
    │   │   写入 GD25Q128 下载区
    │   │
    │   └─ ComponentCRC32Update()
    │       增量计算 CRC32
    │
    ├─ 6. CRC32 校验 (计算值 vs 服务器下发值)
    │
    ├─ 7. FlashFwWriteInfo()
    │   写固件信息头到 GD25Q128 (0x0F0000)
    │
    ├─ 8. DrvEepromWrite(0x00, {0x5A, 0x5A, 0x5A, 0x5A})
    │   写 AT24C02 升级标志
    │
    ├─ 9. DrvEepromWrite(0x04, "1.0.0", 32)
    │   写 AT24C02 版本信息
    │
    ├─ 10. App4GPushData(telemetry, {"fw_state":"UPDATED"})
    │   上报升级成功
    │
    └─ 11. NVIC_SystemReset()
        重启系统
```

### 5.4 OTA 分包接收 (App4GProcessIncoming)

```
串口数据接收
    │
    ├─ 检测到 "v2/fw/response/" 前缀
    │   ├─ 解析 "/chunk/{id},{bytes}," 中的 bytes 数量
    │   ├─ 计算已接收数据偏移
    │   ├─ 进入二进制接收模式 (in_ota_chunk = 1)
    │   └─ 继续接收剩余字节
    │
    ├─ 二进制接收模式 (in_ota_chunk == 1)
    │   ├─ 逐字节存入 ota_data_buf
    │   ├─ ota_bytes_remaining--
    │   └─ 接收完成 → ComponentOtaFeedChunk() → 退出二进制模式
    │
    └─ 普通 JSON 模式
        ├─ 大括号匹配
        └─ JSON 完整 → 解析处理
```

## 6. Bootloader 侧流程

### 6.1 状态机

```
上电/复位
    │
    ▼
┌─────────────────┐
│   INIT          │
│  DrvEepromInit()│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  CHECK_APP      │
│  检查APP有效性   │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
  无效       有效
    │         │
    │         ▼
    │   ┌─────────────────┐
    │   │  CHECK_OTA_FLAG │
    │   │  读AT24C02标志   │
    │   └────────┬────────┘
    │            │
    │       ┌────┴────┐
    │       │         │
    │       ▼         ▼
    │   == 0x5A5A5A5A  其他值
    │       │         │
    │       ▼         │
    │   清除标志       │
    │       │         │
    └───────┼─────────┘
            │
            ▼
┌─────────────────────────┐
│  UPDATE_FROM_EXT_FLASH  │
│  从GD25Q128搬运固件      │
│                         │
│  1. 读固件信息头         │
│  2. 验证CRC32           │
│  3. 擦除内部Flash APP区  │
│  4. 搬运固件数据         │
│  5. 验证新APP有效性      │
└────────────┬────────────┘
             │
        ┌────┴────┐
        │         │
        ▼         ▼
      失败       成功
        │         │
        ▼         │
┌───────────┐     │
│  ERROR    │     │
│  停机     │     │
└───────────┘     │
                  │
                  ▼
          ┌─────────────┐
          │  JUMP_APP   │
          │  跳转到APP   │
          └──────┬──────┘
                 │
                 ▼
           运行新固件
```

### 6.2 详细流程

```
Bootloader 启动
    │
    ├─ 1. INIT 阶段
    │   └─ DrvEepromInit()
    │       初始化 AT24C02 I2C (PB6=SCL, PB7=SDA)
    │
    ├─ 2. CHECK_APP 阶段
    │   └─ BootCheckAppValid(0x08008000)
    │       检查内部 Flash APP 区是否有效
    │
    ├─ 3. CHECK_OTA_FLAG 阶段 (APP有效时)
    │   ├─ DrvEepromRead(0x00, buf, 4)
    │   │   读取 AT24C02 升级标志
    │   │
    │   ├─ 如果 buf == {0x5A, 0x5A, 0x5A, 0x5A}
    │   │   ├─ DrvEepromWrite(0x00, {0xFF,0xFF,0xFF,0xFF}, 4)
    │   │   │   清除升级标志
    │   │   └─ 进入 UPDATE_FROM_EXT_FLASH
    │   │
    │   └─ 如果 buf != {0x5A, 0x5A, 0x5A, 0x5A}
    │       └─ 进入 JUMP_APP
    │
    ├─ 4. UPDATE_FROM_EXT_FLASH 阶段
    │   ├─ FlashFwReadInfo(&info)
    │   │   读取 GD25Q128 固件信息头 (0x0F0000)
    │   │
    │   ├─ FlashFwCheckInfo(&info)
    │   │   验证 magic 字段
    │   │
    │   ├─ FlashFwCalcDownloadCRC32(info.fw_size)
    │   │   计算下载区 CRC32 并与 info.fw_crc32 比较
    │   │
    │   ├─ DrvFlashErase(0x08008000, info.fw_size)
    │   │   擦除内部 Flash APP 区
    │   │
    │   ├─ 循环搬运:
    │   │   FlashFwReadDownload() → DrvFlashWrite()
    │   │   从 GD25Q128 读取 → 写入内部 Flash
    │   │
    │   └─ BootCheckAppValid(0x08008000)
    │       验证新 APP 有效性
    │
    └─ 5. JUMP_APP 阶段
        └─ BootJumpToApp(0x08008000)
            设置 MSP 和 PC，跳转到 APP 执行
```

## 7. 关键代码文件

### App_1.0.0

| 文件 | 用途 |
|------|------|
| `app/app_4g.c` | 4G/MQTT 通信，OTA 分包接收 |
| `app/app_4g.h` | Topic 定义，接口声明 |
| `app/app_task.c` | 任务调度，集成 OTA 任务 |
| `component/component_ota/component_ota.c` | OTA 下载核心逻辑 |
| `component/component_ota/component_ota.h` | OTA 数据结构，AT24C02 地址定义 |
| `component/component_flash_fw/component_flash_fw.c` | GD25Q128 固件存储管理 |
| `component/component_crc32/component_crc32.c` | CRC32 校验算法 |
| `driver/driver_eeprom/driver_eeprom.c` | AT24C02 I2C 驱动 |
| `user/main.c` | 入口，VTOR 偏移设置 |

### Bootloader_OTA

| 文件 | 用途 |
|------|------|
| `boot/boot_main.c` | 启动状态机，OTA 标志检测 |
| `boot/boot_update.c` | 从 GD25Q128 搬运固件到内部 Flash |
| `boot/boot_jump.c` | 跳转 APP 实现 |
| `component/component_flash_fw/component_flash_fw.c` | GD25Q128 固件存储管理 |
| `driver/driver_eeprom/driver_eeprom.c` | AT24C02 I2C 驱动 |
| `driver/driver_flash/driver_flash.c` | 内部 Flash 读写擦除 |
| `driver/driver_gd25q128/driver_gd25q128.c` | GD25Q128 SPI Flash 驱动 |

## 8. 错误处理

| 阶段 | 错误情况 | 处理方式 |
|------|----------|----------|
| OTA 下载 | fw_size 超过 512KB | 返回 EFAIL，上报 FAILED |
| OTA 下载 | 分包请求发送失败 | 返回 EFAIL，上报 FAILED |
| OTA 下载 | 分包响应超时 (10秒) | 返回 EFAIL，上报 FAILED |
| OTA 下载 | GD25Q128 写入失败 | 返回 EFAIL，上报 FAILED |
| OTA 下载 | CRC32 校验不匹配 | 返回 EFAIL，上报 FAILED |
| OTA 下载 | AT24C02 写入失败 | 返回 EFAIL，上报 FAILED |
| Bootloader | GD25Q128 固件信息无效 | 进入 ERROR 状态 |
| Bootloader | CRC32 校验失败 | 进入 ERROR 状态 |
| Bootloader | 内部 Flash 擦写失败 | 进入 ERROR 状态 |
| Bootloader | 新 APP 验证失败 | 进入 ERROR 状态 |

## 9. 测试方法

### 9.1 准备工作

1. 编译 App_1.0.0 工程，生成 bin 文件
2. 编译 Bootloader_OTA 工程，生成 bin 文件
3. 将 Bootloader 烧录到 0x08000000
4. 将 App 烧录到 0x08008000

### 9.2 OTA 测试步骤

1. 启动设备，确认 App 正常运行
2. 准备新版本固件 bin 文件
3. 通过 MQTT 工具发送 OTA 通知到 attributes topic:
   ```json
   {
     "fw_title": "app",
     "fw_version": "1.1.0",
     "fw_size": 12345,
     "fw_checksum": "a1b2c3d4"
   }
   ```
4. 观察串口日志，确认下载流程
5. 设备自动重启后，确认 Bootloader 执行升级
6. 确认新 App 运行正常

### 9.3 验证要点

- [ ] App 收到 OTA 通知后开始下载
- [ ] 分包请求和响应正确
- [ ] GD25Q128 写入成功
- [ ] CRC32 校验通过
- [ ] AT24C02 升级标志写入成功
- [ ] AT24C02 版本信息写入成功
- [ ] 设备重启后 Bootloader 检测到标志
- [ ] 固件从 GD25Q128 搬运到内部 Flash 成功
- [ ] 新 App 正常启动运行
