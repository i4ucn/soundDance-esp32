# 第 4 步：先跑麦克风串口测试

打开：

```text
arduino/01_mic_serial_test/01_mic_serial_test.ino
```

上传后打开串口监视器，波特率 `115200`。

## 成功现象

你会看到类似：

```text
RMS: 0.0042  dBFS: -47.5
RMS: 0.0188  dBFS: -34.5
```

对着麦克风拍手、说话，`RMS` 应该变大，`dBFS` 应该变得没那么负。

## 如果没变化

先别进入网页步骤。请回去检查：

- VDD 是否接 3V3。
- GND 是否接好。
- SCK/WS/SD 是否和代码顶部一致。
- L/R 是否接 GND。
- 你选择的 GPIO 是否真的在开发板排针上。
