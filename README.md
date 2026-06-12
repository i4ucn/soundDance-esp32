# iot101: ESP32 声音波形可视化

一个给 IoT 新手、小朋友、科技爱好者准备的入门项目：用 ESP32-S3 和 INMP441 数字麦克风采集声音，再用浏览器实时看到声音的波形、频谱和估算分贝。

你会做出一个小小的声音实验台：

- 拍手时，波形会突然变高。
- 吹口哨时，频谱会出现很尖的主峰。
- 说话时，波形会像山脉一样变化。
- 晚上可以观察工地噪声的声级走势。
- 早上可以试着捕捉鸟叫的高频特征。

## 项目路线

这个仓库分两步，不一口吃成胖子：

1. `arduino/01_mic_serial_test`：只检查 INMP441 有没有采到声音。
2. `arduino/02_sound_visualizer`：ESP32 开网页和 WebSocket，浏览器实时显示声音。

## 需要的材料

| 材料 | 数量 | 说明 |
| --- | --- | --- |
| ESP32-S3 开发板 | 1 | 例如微雪 ESP32-S3-WROOM-1-N8R16 一类开发板 |
| INMP441 I2S 数字麦克风 | 1 | 注意是数字麦克风，不是模拟麦克风 |
| 杜邦线 | 若干 | 公对母或公对公取决于你的板子 |
| 面包板 MB-102 | 1 | 让接线更清楚 |
| Mac + Arduino IDE | 1 | 已安装 ESP32 core |

## 推荐接线

不同 ESP32-S3 开发板引出的 GPIO 不一样。这个项目默认使用一组比较容易找到的 GPIO：

| INMP441 | ESP32-S3 默认 GPIO | 作用 |
| --- | --- | --- |
| VDD | 3V3 | 供电，只能接 3.3V |
| GND | GND | 地线 |
| SCK | GPIO5 | I2S bit clock |
| WS | GPIO6 | I2S left/right clock |
| SD | GPIO4 | I2S data |
| L/R | GND | 选择左声道 |

如果你的开发板排针上没有 GPIO5/6/4，就选择你板子上真实能看到的普通 GPIO，然后改代码顶部这三行：

```cpp
constexpr uint8_t I2S_BCLK_PIN = 5;
constexpr uint8_t I2S_WS_PIN = 6;
constexpr uint8_t I2S_DIN_PIN = 4;
```

不要优先选择这些引脚：`GPIO0`、`GPIO19`、`GPIO20`、`GPIO43`、`GPIO44`。它们常常和启动、USB、串口有关，新手阶段先避开。

## 从零开始

按这个顺序做：

1. 阅读 [第 1 步：认识零件](docs/01_parts.md)。
2. 阅读 [第 2 步：接线](docs/02_wiring.md)。
3. 打开并上传 `arduino/01_mic_serial_test/01_mic_serial_test.ino`。
4. 串口监视器看到 RMS/dBFS 会随拍手变化后，再进入下一步。
5. 打开并上传 `arduino/02_sound_visualizer/02_sound_visualizer.ino`。
6. Mac 连接 `ESP32-AudioLab` WiFi，密码 `12345678`。
7. 浏览器打开 `http://192.168.4.1`。

如果看不到 WiFi 或上传时报错，先看 [排障指南](docs/06_troubleshooting.md)。

## 为什么先做串口测试

声音可视化项目同时包含四件事：接线、I2S 采样、WiFi、网页。新手最容易卡在“到底是哪一层坏了”。所以第一步只看串口：

- 串口有变化：麦克风和 I2S 大概率正常。
- 串口没变化：先别管网页，回去查接线和引脚。

这就是工程里的小秘密：把大问题切成小问题，一个一个点亮。

## 分贝不是正式噪声计

网页里的 `dBFS` 是数字音频强度，可信；`dB SPL` 是估算值，需要校准后才更接近真实分贝。你可以用手机分贝 App 或声级计做参考，再调整网页里的 `dB 校准偏移`。

## 适合继续扩展的玩法

- 夜间噪声记录：把每分钟最大分贝保存成 CSV。
- 工地噪声提醒：超过阈值时网页变红或发通知。
- 鸟叫探索：观察 2 kHz 到 8 kHz 的能量变化。
- 音乐可视化：加彩色频谱和节拍灯效。
- 外壳与防风：让室外采集更稳定。

## 仓库结构

```text
iot101/
  arduino/
    01_mic_serial_test/
    02_sound_visualizer/
  docs/
  web/
  README.md
```

## 给老师和家长

这个项目涉及 3.3V 低压电路，正常使用比较安全，但仍建议成人帮忙确认接线和 USB 供电。不要把 INMP441 接到 5V，也不要让裸露导线短接。
