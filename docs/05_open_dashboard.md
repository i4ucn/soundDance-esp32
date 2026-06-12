# 第 5 步：打开声音可视化网页

串口测试通过后，打开：

```text
arduino/02_sound_visualizer/02_sound_visualizer.ino
```

上传后打开串口监视器。

## 默认热点模式

默认代码会让 ESP32 创建 WiFi：

| 项目 | 值 |
| --- | --- |
| WiFi 名称 | ESP32-AudioLab |
| 密码 | 12345678 |
| 浏览器地址 | http://192.168.4.1 |

Mac 连接这个 WiFi 后，浏览器打开 `http://192.168.4.1`。

## 连接家里的 WiFi

如果想让 ESP32 加入你家里的 WiFi，修改 `02_sound_visualizer.ino` 顶部：

```cpp
const char *WIFI_SSID = "你的WiFi名";
const char *WIFI_PASSWORD = "你的WiFi密码";
```

这时 ESP32 不会显示 `ESP32-AudioLab` 热点。它会在串口监视器里打印自己的 IP，比如：

```text
WiFi connected: http://192.168.1.23
```

Mac 和 ESP32 必须在同一个 WiFi 下。

## 网页上的指标

| 指标 | 含义 |
| --- | --- |
| 估算声级 | 用 dBFS 加校准偏移得到的估算 dB SPL |
| 数字满幅 | dBFS，数字音频相对满幅的强度 |
| RMS | 一帧声音的平均能量 |
| 主峰 | 当前频谱里最明显的频率 |

吹口哨时，主峰会比较稳定。拍手时，波形会突然变宽变高。
