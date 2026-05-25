# Bootloader 实现记录文档

## 1. 当前实现目标

本 Bootloader 用于 GD32F303 项目的 APP 启动和后续串口升级。

当前阶段已经完成 **Bootloader 跳转 APP 的最小闭环**，后续将继续接入 YMODEM 和 GD25Q128，实现串口接收 APP.bin、外部 Flash 缓存、内部 Flash 搬运和新 APP 启动。

当前 Bootloader 的基础流程如下：

```text
上电运行 Bootloader
        ↓
检查 0x08008000 处 APP 是否有效
        ↓
如果 APP 有效，跳转 APP
        ↓
如果 APP 无效，停留在 Bootloader
        ↓
后续等待串口 YMODEM 升级
```

---

## 2. Flash 分区设计

当前采用 Bootloader + APP 的 Flash 分区方式。

```text
0x08000000  ┌──────────────────────────┐
            │ Bootloader 区 32KB       │
0x08008000  ├──────────────────────────┤
            │ APP 运行区              │
            │ APP 从 0x08008000 开始   │
0x08080000  └──────────────────────────┘
```

工程配置如下：

| 工程 | IROM1 Start | Size | 说明 |
|---|---:|---:|---|
| Bootloader | `0x08000000` | `0x8000` | 负责启动、检查、升级、跳转 |
| APP | `0x08008000` | `0x78000` | 真正运行的应用程序 |

APP 工程中需要设置中断向量表偏移：

```c
SCB->VTOR = 0x08008000;
```

该语句需要放在 APP 工程 `main()` 初始化前面，避免跳转后中断向量仍然指向 Bootloader 区域。

---

## 3. 当前已经完成的内容

### 3.1 Bootloader 工程可以正常启动

上电后可以先运行 Bootloader，并通过串口打印 Bootloader 状态信息。

APP 有效时，现象类似：

```text
BOOT STATE INIT
APP VALID
JUMP TO APP
```

APP 无效时，现象类似：

```text
BOOT STATE INIT
APP INVALID
STAY IN BOOTLOADER
BOOT RUNNING
```

说明 Bootloader 主流程已经可以正常运行。

---

### 3.2 APP 合法性检查已经完成

当前 Bootloader 已经可以检查 APP 是否有效。

检查依据主要有两个：

```text
1. APP 栈顶地址是否合法
2. APP 复位入口地址是否合法
```

判断逻辑：

```c
uint8_t BootCheckAppValid(uint32_t app_addr)
{
    uint32_t app_stack = *(volatile uint32_t *)app_addr;
    uint32_t app_entry = *(volatile uint32_t *)(app_addr + 4U);

    if((app_stack & 0x2FFE0000U) != 0x20000000U)
    {
        return 0;
    }

    if((app_entry & 0xFF000000U) != 0x08000000U)
    {
        return 0;
    }

    return 1;
}
```

其中：

```text
app_addr + 0：保存 APP 初始栈顶地址
app_addr + 4：保存 APP 复位入口地址
```

如果栈顶地址在 SRAM 范围，入口地址在 Flash 范围，则认为 APP 基本有效。

---

### 3.3 Bootloader 跳转 APP 已经成功

当前已经实现 Bootloader 跳转到 APP。

跳转流程：

```text
读取 APP 栈顶地址
        ↓
读取 APP 入口地址
        ↓
关闭中断
        ↓
关闭 SysTick
        ↓
设置 MSP
        ↓
跳转到 APP 入口函数
```

参考实现：

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

当前测试结果：

```text
Bootloader 可以正常跳转 APP
APP 可以正常运行
```

说明以下内容基本正确：

```text
Bootloader 地址配置正确
APP 地址配置正确
APP 向量表偏移正确
APP 栈顶和入口地址正确
BootJumpToApp() 流程正确
```

---

### 3.4 APP 无效时可以停留在 Bootloader

当前 Bootloader 在检测到 APP 无效时，不会错误跳转，而是停留在 Bootloader 中。

当前现象：

```text
APP INVALID
STAY IN BOOTLOADER
BOOT RUNNING
```

该功能很重要。后续如果升级失败、APP 被擦除、APP 没有烧录，Bootloader 都可以保留升级入口。

---

## 4. 当前 Bootloader 主流程

当前 Bootloader 主流程可以整理为：

```c
void BootMainProcess(void)
{
    printf("BOOT STATE INIT\r\n");

    if(BootCheckAppValid(APP_START_ADDR) == 1)
    {
        printf("APP VALID\r\n");
        printf("JUMP TO APP\r\n");

        BootJumpToApp(APP_START_ADDR);
    }
    else
    {
        printf("APP INVALID\r\n");
        printf("STAY IN BOOTLOADER\r\n");

        while(1)
        {
            printf("BOOT RUNNING\r\n");
            Delay_ms(1000);
        }
    }
}
```

当前这个版本属于 **最小跳转版 Bootloader**。

---

## 5. 当前已经验证通过的点

| 验证项 | 状态 |
|---|---|
| Bootloader 从 `0x08000000` 启动 | 已完成 |
| APP 从 `0x08008000` 运行 | 已完成 |
| APP 工程偏移配置 | 已完成 |
| APP 向量表偏移 | 已完成 |
| APP 栈顶检查 | 已完成 |
| APP 入口地址检查 | 已完成 |
| Bootloader 跳转 APP | 已完成 |
| APP 无效时停留 Bootloader | 已完成 |
| 串口打印调试状态 | 已完成 |

---

## 6. 当前还没有完成的内容

虽然当前已经可以跳转 APP，但是还没有形成完整升级闭环。

后续还需要继续完成：

```text
SecureCRT 发送 APP.bin
        ↓
YMODEM 接收
        ↓
GD25Q128 保存
        ↓
搬运到 0x08008000
        ↓
启动新 APP
```

---

## 7. Bootloader 串口升级最小闭环实现

### 7.1 实现目标

当前 Bootloader 已经可以完成 APP 跳转，下一步需要实现串口升级最小闭环。

最小升级闭环流程如下：

```text
SecureCRT 发送 APP.bin
        ↓
YMODEM 接收
        ↓
GD25Q128 保存
        ↓
搬运到 0x08008000
        ↓
启动新 APP
```

该流程的目标是先不加入 AT24C02 升级标志、CRC32 完整校验、备份回退和 OTA，只先验证 Bootloader 是否具备“接收新程序并运行新程序”的能力。

---

### 7.2 总体流程说明

```text
Bootloader 上电运行
        ↓
检查 APP 是否有效
        ↓
如果用户输入 U，进入串口升级模式
        ↓
Bootloader 通过 YMODEM 协议接收 APP.bin
        ↓
接收到的数据先写入 GD25Q128 外部 Flash
        ↓
YMODEM 接收完成后，从 GD25Q128 读回检查
        ↓
擦除内部 Flash 的 APP 区
        ↓
将 GD25Q128 中的 APP.bin 搬运到 0x08008000
        ↓
检查 0x08008000 处 APP 是否有效
        ↓
跳转运行新 APP
```

---

### 7.3 为什么不直接写内部 Flash

YMODEM 接收 APP.bin 时，不建议一开始就直接写入内部 Flash。

原因如下：

```text
1. 串口传输过程中可能中断
2. YMODEM 接收过程中可能出现丢包或 CRC 错误
3. 如果直接擦除内部 APP 区，失败后原 APP 就没了
4. 先写 GD25Q128，可以先缓存完整固件
5. 接收完成后确认数据正确，再搬运到内部 Flash
```

所以当前设计采用：

```text
YMODEM → GD25Q128 → 内部 Flash APP 区
```

而不是：

```text
YMODEM → 内部 Flash APP 区
```

---

### 7.4 SecureCRT 发送 APP.bin

Bootloader 进入升级模式后，需要主动发送字符 `C`，通知上位机使用 CRC 模式开始传输。

基本流程：

```text
1. 复位开发板，进入 Bootloader
2. 串口打印：Press U to update
3. 用户在 SecureCRT 输入 U
4. Bootloader 进入 YMODEM 接收模式
5. Bootloader 周期发送字符 C
6. SecureCRT 选择 Transfer -> Send Ymodem
7. 选择 APP.bin
8. 开始发送
```

Bootloader 打印示例：

```text
BOOT RUNNING
Press U to update
Enter YMODEM mode
Send C...
```

---

### 7.5 YMODEM 接收内容

YMODEM 传输时主要有三类数据：

```text
第 0 包：文件信息包
第 1 包及以后：APP.bin 数据包
EOT：传输结束
```

#### 7.5.1 第 0 包

第 0 包不是真正的 APP 程序数据，而是文件信息。

内容一般是：

```text
文件名\0文件大小\0
```

例如：

```text
app.bin\086240\0
```

Bootloader 需要从第 0 包中解析出：

```text
文件名：app.bin
文件大小：86240
```

调试打印示例：

```text
[YMODEM] file name: app.bin
[YMODEM] file size: 86240
```

---

#### 7.5.2 数据包

从第 1 包开始，才是真正的 APP.bin 数据。

数据包可能是：

```text
SOH 包：128 字节数据
STX 包：1024 字节数据
```

每个数据包格式：

```text
SOH/STX + 包序号 + 包序号反码 + 数据区 + CRC16高字节 + CRC16低字节
```

Bootloader 需要检查：

```text
1. 包头是否为 SOH 或 STX
2. 包序号是否正确
3. 包序号和反码是否互补
4. CRC16 是否正确
5. 正确后写入 GD25Q128
6. 写入成功后回复 ACK
7. 错误时回复 NAK
```

---

#### 7.5.3 EOT 结束包

当 SecureCRT 发送完 APP.bin 后，会发送 `EOT` 表示传输结束。

Bootloader 收到 EOT 后，需要回复 ACK，并结束 YMODEM 接收。

调试打印示例：

```text
[YMODEM] EOT received
[YMODEM] receive finish
```

---

### 7.6 GD25Q128 保存 APP.bin

YMODEM 收到 APP.bin 的数据包后，不直接运行，也不直接写内部 Flash，而是先写入 GD25Q128。

建议下载区地址：

```c
#define EXT_FW_DOWNLOAD_ADDR      0x000000UL
```

写入逻辑：

```text
第 1 个数据包 → GD25Q128 0x000000
第 2 个数据包 → GD25Q128 0x000400
第 3 个数据包 → GD25Q128 0x000800
后续依次累加
```

如果使用 1024 字节包，每次写入地址增加：

```text
0x400
```

也就是 1024 字节。

如果使用 128 字节包，每次写入地址增加：

```text
0x80
```

也就是 128 字节。

---

### 7.7 写入 GD25Q128 前的擦除

GD25Q128 写入前必须先擦除，因为 Flash 只能从 `1` 写成 `0`，不能直接从 `0` 写回 `1`。

所以在 YMODEM 正式写数据前，需要根据文件大小擦除对应扇区。

例如：

```c
uint32_t erase_size;
uint32_t erase_addr;

erase_size = file_size;
erase_addr = EXT_FW_DOWNLOAD_ADDR;

while(erase_size > 0)
{
    DrvGD25Q128EraseSector(erase_addr);
    erase_addr += 4096;

    if(erase_size > 4096)
    {
        erase_size -= 4096;
    }
    else
    {
        erase_size = 0;
    }
}
```

注意：

```text
GD25Q128 一个扇区是 4KB
页写大小是 256 字节
写入前必须擦除
页写不能跨 256 字节页
```

---

### 7.8 YMODEM 写 GD25Q128 的核心逻辑

可以理解成：

```c
fw_write_addr = EXT_FW_DOWNLOAD_ADDR;
received_size = 0;

while(ymodem_is_receiving)
{
    if(receive_packet_ok)
    {
        DrvGD25Q128Write(fw_write_addr, packet_data, packet_len);

        fw_write_addr += packet_len;
        received_size += packet_len;

        SendAck();
    }
    else
    {
        SendNak();
    }
}
```

实际写入时要注意最后一包可能会带填充数据，不能全部算入有效文件大小。

所以应该限制：

```c
if(received_size + packet_len > file_size)
{
    valid_len = file_size - received_size;
}
else
{
    valid_len = packet_len;
}
```

然后只写有效长度：

```c
DrvGD25Q128Write(fw_write_addr, packet_data, valid_len);
```

---

### 7.9 GD25Q128 读回验证

YMODEM 接收完成后，先不要急着擦内部 Flash。

应该先从 GD25Q128 读回前 8 字节，确认 APP.bin 开头是否正确。

读取示例：

```c
uint8_t check_buf[8];

DrvGD25Q128Read(EXT_FW_DOWNLOAD_ADDR, check_buf, 8);

printf("APP HEAD: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
       check_buf[0], check_buf[1], check_buf[2], check_buf[3],
       check_buf[4], check_buf[5], check_buf[6], check_buf[7]);
```

正常情况下，前 8 字节应该分别是：

```text
前 4 字节：APP 初始栈顶地址，一般为 0x2000xxxx
后 4 字节：APP 复位入口地址，一般为 0x08008xxx
```

例如小端格式显示可能类似：

```text
00 00 01 20  51 83 00 08
```

它表示：

```text
初始栈顶地址：0x20010000
复位入口地址：0x08008351
```

如果读出来是：

```text
FF FF FF FF FF FF FF FF
```

说明可能没有写进去。

如果读出来是：

```text
00 00 00 00 00 00 00 00
```

说明数据可能写错或擦写流程异常。

如果复位入口不是 `0x08008xxx`，说明 APP.bin 可能不是按 `0x08008000` 偏移编译的。

---

### 7.10 搬运到内部 Flash

确认 GD25Q128 中的 APP.bin 基本正确后，就可以搬运到内部 Flash。

搬运目标地址：

```c
#define APP_START_ADDR      0x08008000U
```

基本流程：

```text
1. 擦除内部 Flash APP 区
2. 从 GD25Q128 读取一块数据
3. 写入内部 Flash 0x08008000
4. 重复读取和写入，直到 file_size 写完
5. 检查 APP 是否有效
6. 跳转 APP
```

搬运伪代码：

```c
uint8_t BootCopyFirmwareToApp(uint32_t ext_addr, uint32_t app_addr, uint32_t fw_size)
{
    uint8_t buf[512];
    uint32_t offset = 0;
    uint32_t write_len;

    DrvFlashErase(app_addr, fw_size);

    while(offset < fw_size)
    {
        if((fw_size - offset) >= sizeof(buf))
        {
            write_len = sizeof(buf);
        }
        else
        {
            write_len = fw_size - offset;
        }

        DrvGD25Q128Read(ext_addr + offset, buf, write_len);

        if(DrvFlashWrite(app_addr + offset, buf, write_len) != 0)
        {
            return 1;
        }

        offset += write_len;
    }

    return 0;
}
```

---

### 7.11 搬运后检查 APP

搬运完成后，必须重新检查内部 Flash 中的 APP 是否有效。

```c
if(BootCheckAppValid(APP_START_ADDR) == 1)
{
    printf("NEW APP VALID\r\n");
    BootJumpToApp(APP_START_ADDR);
}
else
{
    printf("NEW APP INVALID\r\n");
    printf("STAY IN BOOTLOADER\r\n");
}
```

判断成功后，再跳转 APP。

不能接收完就直接跳转，必须检查：

```text
栈顶地址是否在 SRAM
入口地址是否在 Flash
```

---

### 7.12 启动新 APP

如果搬运成功，Bootloader 跳转到新 APP。

期望打印：

```text
BOOT RUNNING
Enter YMODEM mode
[YMODEM] file name: app.bin
[YMODEM] file size: 86240
[YMODEM] receive finish
[FLASH] erase app area ok
[FLASH] copy firmware ok
NEW APP VALID
JUMP TO NEW APP
APP RUNNING FROM 0x08008000
```

这样说明最小升级闭环已经跑通。

---

## 8. 最小闭环函数职责划分

### 8.1 `component_ymodem.c`

负责 YMODEM 协议本身。

主要职责：

```text
发送字符 C
接收数据包
解析文件名
解析文件大小
校验 CRC16
回复 ACK / NAK
识别 EOT
```

建议接口：

```c
uint8_t BootYmodemReceiveToFlash(uint32_t save_addr, uint32_t *file_size);
```

说明：

```text
save_addr：APP.bin 保存到 GD25Q128 的起始地址
file_size：接收到的 APP.bin 文件大小
```

---

### 8.2 `driver_gd25q128.c`

负责外部 Flash 操作。

主要职责：

```text
读取 ID
擦除扇区
页写
连续读
等待 Busy
```

建议接口：

```c
uint8_t DrvGD25Q128ReadID(uint8_t *mid, uint8_t *type, uint8_t *capacity);
uint8_t DrvGD25Q128EraseSector(uint32_t addr);
uint8_t DrvGD25Q128Write(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t DrvGD25Q128Read(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t DrvGD25Q128WaitBusy(void);
```

---

### 8.3 `driver_flash.c`

负责内部 Flash 操作。

主要职责：

```text
擦除内部 APP 区
写入内部 APP 区
读取内部 Flash 数据
```

建议接口：

```c
uint8_t DrvFlashErase(uint32_t addr, uint32_t size);
uint8_t DrvFlashWrite(uint32_t addr, uint8_t *data, uint32_t len);
uint32_t DrvFlashReadWord(uint32_t addr);
```

---

### 8.4 `boot_update.c`

负责升级流程调度。

它不负责具体协议，也不负责底层擦写细节，而是负责把流程串起来。

主要职责：

```text
调用 YMODEM 接收 APP.bin
调用 GD25Q128 保存固件
调用内部 Flash 驱动擦写 APP 区
调用 BootCheckAppValid 检查 APP
调用 BootJumpToApp 启动 APP
```

建议接口：

```c
uint8_t BootSerialUpdateProcess(void);
uint8_t BootCopyFirmwareToApp(uint32_t ext_addr, uint32_t app_addr, uint32_t fw_size);
```

---

## 9. 最小升级闭环状态机

可以先设计一个简单状态机：

```c
typedef enum
{
    BOOT_UPDATE_IDLE = 0,
    BOOT_UPDATE_WAIT_YMODEM,
    BOOT_UPDATE_RECEIVING,
    BOOT_UPDATE_CHECK_EXT_FLASH,
    BOOT_UPDATE_ERASE_APP,
    BOOT_UPDATE_COPY_APP,
    BOOT_UPDATE_CHECK_APP,
    BOOT_UPDATE_JUMP_APP,
    BOOT_UPDATE_FAIL,

} BootUpdateState_e;
```

状态流转：

```text
IDLE
 ↓
WAIT_YMODEM
 ↓
RECEIVING
 ↓
CHECK_EXT_FLASH
 ↓
ERASE_APP
 ↓
COPY_APP
 ↓
CHECK_APP
 ↓
JUMP_APP
```

失败时进入：

```text
BOOT_UPDATE_FAIL
```

失败后暂时先停留在 Bootloader：

```text
升级失败
        ↓
打印错误信息
        ↓
停留 Bootloader
        ↓
等待重新发送 APP.bin
```

---

## 10. 当前阶段验收标准

这个阶段完成后，应该满足下面几个现象：

```text
1. Bootloader 可以进入 YMODEM 模式
2. SecureCRT 可以正常发送 APP.bin
3. Bootloader 可以解析文件名和文件大小
4. APP.bin 可以写入 GD25Q128
5. GD25Q128 读回前 8 字节正确
6. Bootloader 可以擦除 0x08008000 后的 APP 区
7. Bootloader 可以把 GD25Q128 中的 APP 搬运到 0x08008000
8. BootCheckAppValid(0x08008000) 返回有效
9. BootJumpToApp(0x08008000) 可以启动新 APP
```

最终串口现象：

```text
BOOT RUNNING
Press U to update
Enter YMODEM mode
[YMODEM] file name: app.bin
[YMODEM] file size: xxxx
[YMODEM] receive finish
[GD25Q128] read back ok
[FLASH] erase app ok
[FLASH] copy app ok
NEW APP VALID
JUMP TO APP
APP RUNNING
```

---

## 11. 当前阶段不加入的功能

为了先跑通最小闭环，以下功能暂时不加：

```text
AT24C02 升级标志
APP CRC32 总校验
旧 APP 备份
升级失败回退
4G OTA
版本号判断
断点续传
加密验签
```

这些功能后面再补。当前阶段只验证：

```text
能不能通过串口发送一个 APP.bin，并让 Bootloader 启动这个新 APP。
```

---

## 12. 当前 Bootloader 状态总结

当前状态：

```text
Bootloader 最小跳转功能已经完成
APP 可以从 0x08008000 正常运行
APP 无效时可以停留在 Bootloader
```

当前剩余主线：

```text
YMODEM + GD25Q128 + 内部 Flash 搬运
```

当前不急着做的增强功能：

```text
AT24C02 升级标志
CRC32 完整校验
旧 APP 备份
升级失败回退
4G OTA
```

这些功能可以等最小升级闭环跑通后再逐步补充。

---

## 13. 当前开发结论

当前 Bootloader 已经完成了最基础、最关键的跳转验证。说明工程地址划分、APP 偏移、栈顶检查、入口地址检查和跳转函数都已经正确。

下一步只需要集中完成：

```text
SecureCRT 发送 APP.bin
        ↓
YMODEM 接收
        ↓
GD25Q128 保存
        ↓
搬运到 0x08008000
        ↓
启动新 APP
```

这条主线完成后，Bootloader 的 V1 版本就基本成型。
