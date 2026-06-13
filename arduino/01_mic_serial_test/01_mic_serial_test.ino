#include <Arduino.h>
#include "ESP_I2S.h"

// If your board does not expose these GPIOs, change them to pins you can see.
constexpr uint8_t I2S_BCLK_PIN = 5;  // INMP441 SCK
constexpr uint8_t I2S_WS_PIN = 6;    // INMP441 WS
constexpr uint8_t I2S_DIN_PIN = 4;   // INMP441 SD

// L/R to GND means left channel. L/R to 3V3 means right channel.
constexpr int8_t MIC_I2S_SLOT = I2S_STD_SLOT_LEFT;

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t SAMPLE_COUNT = 512;
constexpr uint8_t PCM_GAIN_SHIFT = 14;
constexpr uint32_t I2S_READ_TIMEOUT_MS = 100;

I2SClass i2s;
int32_t rawSamples[SAMPLE_COUNT];
uint32_t lastNoSampleLogMs = 0;

void setup() {
  Serial.begin(115200);
  uint32_t waitStartMs = millis();
  while (!Serial && millis() - waitStartMs < 3000) {
    delay(10);
  }
  delay(800);

  Serial.println();
  Serial.println("iot101 mic serial test");
  Serial.println("If RMS changes when you clap, the microphone is alive.");
  Serial.print("Pins: SCK GPIO");
  Serial.print(I2S_BCLK_PIN);
  Serial.print(", WS GPIO");
  Serial.print(I2S_WS_PIN);
  Serial.print(", SD GPIO");
  Serial.print(I2S_DIN_PIN);
  Serial.print(", L/R slot ");
  Serial.println(MIC_I2S_SLOT == I2S_STD_SLOT_LEFT ? "LEFT" : "RIGHT");

  i2s.setPins(I2S_BCLK_PIN, I2S_WS_PIN, -1, I2S_DIN_PIN);
  i2s.setTimeout(I2S_READ_TIMEOUT_MS);
  bool ok = i2s.begin(
    I2S_MODE_STD,
    SAMPLE_RATE,
    I2S_DATA_BIT_WIDTH_32BIT,
    I2S_SLOT_MODE_MONO,
    MIC_I2S_SLOT
  );

  if (!ok) {
    Serial.println("I2S init failed. Check board target and pin numbers.");
    while (true) delay(1000);
  }

  Serial.println("I2S microphone ready.");
}

void loop() {
  size_t bytesRead = i2s.readBytes(reinterpret_cast<char *>(rawSamples), sizeof(rawSamples));
  size_t samplesRead = bytesRead / sizeof(rawSamples[0]);
  if (samplesRead == 0) {
    if (millis() - lastNoSampleLogMs >= 1000) {
      lastNoSampleLogMs = millis();
      Serial.println("No I2S samples. Check VDD=3V3, GND, SCK=GPIO5, WS=GPIO6, SD=GPIO4, L/R=GND.");
    }
    return;
  }

  int64_t sum = 0;
  for (size_t i = 0; i < samplesRead; i++) {
    sum += rawSamples[i] >> PCM_GAIN_SHIFT;
  }
  int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(samplesRead));

  double squareSum = 0.0;
  int32_t peak = 0;
  for (size_t i = 0; i < samplesRead; i++) {
    int32_t sample = (rawSamples[i] >> PCM_GAIN_SHIFT) - mean;
    peak = max(peak, abs(sample));
    float normalized = static_cast<float>(sample) / 32768.0f;
    squareSum += static_cast<double>(normalized) * normalized;
  }

  float rms = sqrt(squareSum / samplesRead);
  float dbfs = rms > 0.000001f ? 20.0f * log10f(rms) : -100.0f;

  Serial.print("RMS: ");
  Serial.print(rms, 5);
  Serial.print("  dBFS: ");
  Serial.print(dbfs, 1);
  Serial.print("  peak: ");
  Serial.println(peak);

  delay(120);
}
