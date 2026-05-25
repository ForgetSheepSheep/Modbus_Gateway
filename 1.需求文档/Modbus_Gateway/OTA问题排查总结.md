# OTA 问题排查总结

## 1. 当前结论

本次 OTA 主流程已经打通：

1. App 通过 4G 收到云平台 OTA 通知。
2. App 按 chunk 下载固件。
3. App 写入外部 GD25Q128 Flash。
4. App 计算固件 CRC，并兼容云平台字节序相反的 CRC。
5. App 写入外部 Flash 固件信息头。
6. App 写入 EEPROM OTA flag 和 OTA 元信息。
7. Bootloader 检测 OTA flag。
8. Bootloader 初始化外部 Flash。
9. Bootloader 校验外部 Flash 固件信息头。
10. Bootloader 校验外部 Flash 固件 CRC。
11. Bootloader 擦除内部 APP 区。
12. Bootloader 从外部 Flash 拷贝新 APP 到 `0x08004000`。
13. Bootloader 校验新 APP 向量表。
14. Bootloader 清除 OTA flag。
15. Bootloader 跳转新 APP。

最终有效日志：

```text
[OTA] crc32 matched by swapped platform checksum
[OTA] fw info written
[OTA] rebooting...

BOOT RUNNING
OTA FLAG DETECTED
[UPDATE] check external flash firmware...
FLASH ID: 0xC84018
[UPDATE] info magic=0x46574D47 size=24376 crc=0x696D9A53 ver=0 addr=0x00000000
[FLASH] erase app area...
[FLASH] copy firmware...
NEW APP VALID
OTA FLAG CLEARED
JUMP TO NEW APP
```

## 2. 问题列表

### 2.1 4G 初始化偶发失败

现象：

```text
[4G] step 0 failed state=3 expect=OK, retry
```

分析：

4G 模块初始化时偶发没有按预期返回 `OK`，但后续能重试成功：

```text
[4G] init ok
```

结论：

该问题不是 OTA 主故障，属于初始化阶段的偶发重试问题。

### 2.2 4G UART 数据包处理不稳

现象：

OTA chunk 响应是二进制数据，且数据量较大。串口日志中能看到部分打印被拆成多段：

```text
[OTA] request chunk topic=v2/fw/request/37
304/chunk/1 bytes=256
```

分析：

这类拆行本身是串口工具显示问题，不等于 MQTT topic 或数据包真的断了。真正需要关注的是：

1. UART ring buffer 是否溢出。
2. OTA 响应是否匹配当前 request id。
3. chunk id 是否匹配。
4. chunk 数据长度是否正确。
5. 二进制 payload 是否被截断。

处理：

增加 OTA 响应匹配日志：

```text
[4G-OTA] response matched id=37304 chunk=95 bytes=56
```

结论：

后续日志显示每个 chunk 都能正确 matched，说明 OTA 响应包匹配正常。

### 2.3 4G UART ring buffer 太小

现象：

OTA 连续下载时，每片 chunk 为 256 字节，再加上 MQTT topic、AT 返回头、JSON 属性包等内容，原 ring buffer 偏小，存在溢出风险。

风险：

如果 ring buffer 溢出，会导致：

1. OTA 数据丢字节。
2. JSON 解析失败。
3. MQTT 响应匹配失败。
4. chunk payload 长度错误。
5. 最终 CRC mismatch。

处理：

1. 扩大 4G UART ring buffer。
2. 增加 UART drop 计数。
3. 每个 chunk 打印 `uart_drop`。

验证日志：

```text
[OTA] chunk_id=95 recv_len=56 chunk_crc=0xD253F2C4 uart_drop=0 write_addr=0x00005F00
```

结论：

当前测试中 `uart_drop=0`，说明没有 UART 丢字节。

### 2.4 OTA chunk 下载 CRC mismatch

现象：

```text
[OTA] crc32 mismatch!
[OTA] download failed
```

早期表现：

```text
crc_calc=0x8368CE19 crc_expect=0x5F5FB711
crc_calc=0x0D97B8D3 crc_expect=0x56591D39
crc_calc=0x416B743C crc_expect=0x539A6D69
```

分析过程：

曾排查过以下方向：

1. CRC32 算法是否不同。
2. 云平台 CRC 是否字节序相反。
3. 外部 Flash 写入是否错误。
4. EEPROM 元信息是否错误。
5. UART 是否丢包。
6. 最后一包是否下载错误。

最终根因：

最后一片 chunk 请求长度错误。

### 2.5 最后一片 chunk 请求长度错误

现象：

固件大小：

```text
fw_size=24376
total_chunks=96
last_chunk_len=56
```

之前最后一片请求：

```text
chunk/95 bytes=56
```

根因：

云平台按请求里的 `bytes` 作为 chunk size 来计算 offset。如果最后一片请求 `bytes=56`，平台会按 `95 * 56` 去取数据，而不是按 `95 * 256` 去取数据，导致最后一片数据错误。

正确做法：

所有 chunk 请求都固定：

```text
bytes=256
```

即使最后一片只有 56 字节，也请求 256，收到后本地只写有效长度 56。

修复后日志：

```text
[OTA] request chunk topic=v2/fw/request/37304/chunk/95 bytes=256
[4G-OTA] response matched id=37304 chunk=95 bytes=56
[OTA] chunk_id=95 recv_len=56 chunk_crc=0xD253F2C4 uart_drop=0 write_addr=0x00005F00
```

结论：

这是 App 下载阶段 CRC mismatch 的主要根因。

### 2.6 云平台 CRC 字节序相反

现象：

本地标准 CRC32：

```text
0x696D9A53
```

云平台显示 CRC：

```text
0x539A6D69
```

两者关系：

```text
0x696D9A53 -> byte swap -> 0x539A6D69
```

处理：

App 端兼容两种比较：

1. 标准 CRC32 直接相等。
2. 标准 CRC32 与云平台 CRC byte swap 后相等。

但写入外部 Flash 信息头时，仍保存标准 CRC32：

```text
fw_crc32=0x696D9A53
```

验证日志：

```text
[OTA] crc_diag ieee=0x696D9A53 ieee_swap=0x539A6D69 ieee_no_xor=0x969265AC mpeg2=0x8744CB3C expect=0x539A6D69
[OTA] crc32 matched by swapped platform checksum: expect=0x539A6D69 swap=0x696D9A53
```

### 2.7 外部 Flash 写入确认

现象：

需要确认数据是下载错、UART 丢包、还是 Flash 写错。

处理：

增加：

1. 每片 `chunk_crc`。
2. 每片写入后读回校验。
3. 固件整体 `crc_calc`。
4. 外部 Flash 读回 `crc_flash`。
5. 固件头部 dump。

验证日志：

```text
[FLASH_FW] dump offset=0x00000000 len=32: 50 2E 00 20 45 41 00 08 ...
[OTA] fw_size=24376 received_bytes=24376 total_chunks=96 last_chunk_len=56 crc_calc=0x696D9A53 crc_flash=0x696D9A53
```

结论：

外部 GD25Q128 下载区数据写入正确。

### 2.8 EEPROM OTA 信息写入确认

现象：

App 下载成功后需要写 EEPROM，供 Bootloader 检测 OTA。

写入信息：

```text
title=app2路
version=20
size=24376
checksum=539a6d69
```

Bootloader 读取日志：

```text
[OTA] title: app2路
[OTA] version: 20
[OTA] checksum: 539a6d69
[OTA] size: 24376
```

结论：

EEPROM OTA flag 和 OTA 元信息写入、读取正常。

### 2.9 Bootloader 未初始化外部 Flash

现象：

App 下载成功并重启后，Bootloader 失败：

```text
OTA FLAG DETECTED
[UPDATE] check external flash firmware...
[UPDATE] firmware info invalid
UPDATE FROM EXT FLASH FAIL
BOOT STATE ERROR
```

根因：

Bootloader 进入 `BootUpdateFromExternalFlash()` 后直接读取外部 Flash 固件信息头，但没有先调用 `FlashFwInit()` 初始化 GD25Q128/SPI。

修复：

在 Bootloader 更新入口增加：

```c
if(FlashFwInit() != ESUCCESS)
{
    printf("[UPDATE] external flash init error\r\n");
    return EFAIL;
}
```

验证日志：

```text
[UPDATE] check external flash firmware...
FLASH ID: 0xC84018
```

结论：

Bootloader 外部 Flash 初始化问题已修复。

### 2.10 Bootloader 固件信息头验证

现象：

需要确认 Bootloader 是否能读到 App 写入的外部 Flash 信息头。

新增日志：

```text
[UPDATE] info magic=0x46574D47 size=24376 crc=0x696D9A53 ver=0 addr=0x00000000
[FLASH_FW] raw dump addr=0x000F0000 len=32: 47 4D 57 46 38 5F 00 00 53 9A 6D 69 00 00 00 00 ...
```

字段解析：

| 字段 | 值 | 含义 |
| --- | --- | --- |
| magic | `0x46574D47` | `'FWMG'` |
| size | `24376` | 固件大小 |
| crc | `0x696D9A53` | 标准 CRC32 |
| version | `0` | 当前未使用 |
| addr | `0x00000000` | 外部 Flash 下载区地址 |

结论：

Bootloader 已能正确读取固件信息头。

### 2.11 Bootloader 过早清 OTA flag

问题：

之前 Bootloader 检测到 OTA flag 后立即清除：

```text
OTA FLAG DETECTED
OTA FLAG CLEARED
```

风险：

如果后续外部 Flash 校验失败或 APP 拷贝失败，OTA flag 已经没了，设备无法自动重试更新。

修复：

改成只有更新成功后才清除 OTA flag：

```text
NEW APP VALID
OTA FLAG CLEARED
```

结论：

更新失败时可以保留 OTA flag，方便复位后继续重试。

### 2.12 Bootloader APP 拷贝成功

最终验证日志：

```text
[FLASH] erase app area...
[FLASH] copy firmware...
NEW APP VALID
OTA FLAG CLEARED
JUMP TO NEW APP
JUMP TO APP
```

结论：

Bootloader 已经成功完成：

1. 擦除内部 APP 区。
2. 从外部 Flash 拷贝新 APP。
3. 校验新 APP 向量表。
4. 跳转新 APP。

### 2.13 升级后仍显示 APP 1.0

现象：

OTA 成功后仍打印：

```text
[APP 1.0] started @ 0x08004000
```

原因：

`App_2.0/user/main.c` 中调试字符串仍写死为：

```c
printf("[APP 1.0] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
```

结论：

这不是升级失败，只是调试打印没有改。

后续建议改成：

```c
printf("[APP 2.0] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
```

或：

```c
printf("[APP 20] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
```

## 3. 本次关键修改

### 3.1 App OTA 下载侧

修改点：

1. 所有 OTA chunk 请求固定 `bytes=256`。
2. 最后一片只在本地截取有效长度。
3. 增加 chunk id / request id 匹配日志。
4. 增加 chunk CRC 日志。
5. 增加 UART drop 计数。
6. 扩大 4G UART ring buffer。
7. 增加外部 Flash 写入后读回校验。
8. 增加整体 `crc_calc` 和 `crc_flash` 对比。
9. 增加 CRC32 诊断输出。
10. 兼容云平台 swapped CRC。

### 3.2 Bootloader 更新侧

修改点：

1. `BootUpdateFromExternalFlash()` 入口先调用 `FlashFwInit()`。
2. 打印外部 Flash JEDEC ID。
3. 打印固件信息头字段。
4. dump 外部 Flash 信息区原始 32 字节。
5. CRC 错误时打印 `calc` 和 `expect`。
6. OTA flag 改为更新成功后再清除。

## 4. 当前剩余事项

### 4.1 修改 App_2.0 启动打印

文件：

```text
App_2.0/user/main.c
```

当前：

```c
printf("[APP 1.0] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
```

建议：

```c
printf("[APP 2.0] started @ 0x%08X\r\n", (unsigned int)APP_START_ADDR);
```

### 4.2 后续可清理调试日志

当前 OTA 已经跑通，后续量产前可以减少以下日志：

1. 每片 `chunk_crc`。
2. 每片 `uart_drop`。
3. CRC diag。
4. Flash raw dump。
5. Bootloader info dump。

建议保留关键错误日志：

1. OTA notify 信息。
2. 下载成功/失败。
3. CRC 成功/失败。
4. Bootloader 更新成功/失败。
5. APP 版本号。

## 5. 最终判断

本次问题不是单一 bug，而是多个问题叠加：

1. OTA chunk 最后一片请求长度错误。
2. 云平台 CRC 字节序与本地标准 CRC32 相反。
3. 4G UART ring buffer 有溢出风险。
4. 二进制 OTA 数据包处理需要更明确的匹配和长度校验。
5. Bootloader 更新前没有初始化外部 Flash。
6. Bootloader 过早清 OTA flag。
7. App_2.0 调试版本字符串未修改。

目前 OTA 主链路已经验证通过，剩余主要是版本打印和调试日志清理。
