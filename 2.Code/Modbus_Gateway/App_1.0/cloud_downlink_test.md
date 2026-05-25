# 云平台下发数据格式测试

## 订阅主题

设备订阅 RPC 下发主题：

```text
v1/devices/me/rpc/request/+
```

## 下发 JSON 格式

支持两种格式。

### ThingsBoard RPC 格式

```json
{
  "method": "modbus_ctrl",
  "params": {
    "addr": 2,
    "ctrl": 31
  }
}
```

### 简化格式

```json
{
  "addr": 2,
  "ctrl": 31
}
```

## 字段说明

`addr`：Modbus 从机地址。

| addr | 从机 | 说明 |
|---:|---|---|
| `1` / `"0x01"` | ADC 从机 | LED1-3、BEEP1-2 |
| `2` / `"0x02"` | IO 从机 | LED1-3、RELAY1-2 |
| `3` / `"0x03"` | 温湿度从机 | LED1-3、BEEP1-2 |

`ctrl`：5 路输出位图，写入寄存器 `0000-0004`。

| bit | 值 | ADC/温湿度 | IO |
|---:|---:|---|---|
| bit0 | `0x01` | LED1 | LED1 |
| bit1 | `0x02` | LED2 | LED2 |
| bit2 | `0x04` | LED3 | LED3 |
| bit3 | `0x08` | BEEP1 | RELAY1 |
| bit4 | `0x10` | BEEP2 | RELAY2 |

## 测试用例

### IO 从机 `02`

全部打开：

```json
{"method":"modbus_ctrl","params":{"addr":2,"ctrl":31}}
```

全部关闭：

```json
{"method":"modbus_ctrl","params":{"addr":2,"ctrl":0}}
```

只开 LED1：

```json
{"method":"modbus_ctrl","params":{"addr":2,"ctrl":1}}
```

只开 RELAY1：

```json
{"method":"modbus_ctrl","params":{"addr":2,"ctrl":8}}
```

LED1 + RELAY1 打开：

```json
{"method":"modbus_ctrl","params":{"addr":2,"ctrl":9}}
```

### ADC 从机 `01`

全部打开：

```json
{"method":"modbus_ctrl","params":{"addr":1,"ctrl":31}}
```

全部关闭：

```json
{"method":"modbus_ctrl","params":{"addr":1,"ctrl":0}}
```

只开 BEEP1：

```json
{"method":"modbus_ctrl","params":{"addr":1,"ctrl":8}}
```

### 温湿度从机 `03`

全部打开：

```json
{"method":"modbus_ctrl","params":{"addr":3,"ctrl":31}}
```

全部关闭：

```json
{"method":"modbus_ctrl","params":{"addr":3,"ctrl":0}}
```

只开 LED3：

```json
{"method":"modbus_ctrl","params":{"addr":3,"ctrl":4}}
```

## 设备上报格式

设备每 5 秒轮流上报一个从机数据到：

```text
v1/devices/me/telemetry
```

IO：

```json
{"addr":"02","io":0}
```

ADC：

```json
{"addr":"01","adc1":0,"adc2":0}
```

温湿度：

```json
{"addr":"03","temp":0,"humi":0}
```
