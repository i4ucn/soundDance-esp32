# 第 3 步：Arduino IDE 设置

## 开发板

在 Arduino IDE 里选择：

- `ESP32S3 Dev Module`

如果你的 Arduino IDE 里有更准确的 Waveshare ESP32-S3 选项，也可以选对应选项。拿不准时，先用 `ESP32S3 Dev Module`。

## 推荐工具设置

| 选项 | 推荐值 |
| --- | --- |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB |
| PSRAM | OPI PSRAM 或 Enabled |
| Upload Speed | 460800 或 921600 |
| Port | 选择插入 ESP32 后出现的端口 |

如果上传不稳定，把 Upload Speed 降到 `460800`。

## 串口监视器

打开串口监视器，波特率选择：

```text
115200
```

你会看到 ESP32 打印出来的启动信息、WiFi 地址、WebSocket 连接状态。
