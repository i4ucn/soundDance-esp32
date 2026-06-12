#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "ESP_I2S.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include "WebPage.h"

// Leave WIFI_SSID empty to make ESP32 create the ESP32-AudioLab hotspot.
// Fill both values if you want ESP32 to join your home WiFi instead.
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

const char *AP_SSID = "ESP32-AudioLab";
const char *AP_PASSWORD = "12345678";

// If your board does not expose these GPIOs, change them to pins you can see.
constexpr uint8_t I2S_BCLK_PIN = 5;  // INMP441 SCK
constexpr uint8_t I2S_WS_PIN = 6;    // INMP441 WS
constexpr uint8_t I2S_DIN_PIN = 4;   // INMP441 SD

// L/R to GND means left channel. L/R to 3V3 means right channel.
constexpr int8_t MIC_I2S_SLOT = I2S_STD_SLOT_LEFT;

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t FRAME_SAMPLES = 512;
constexpr uint8_t PCM_GAIN_SHIFT = 14;
constexpr float DB_SPL_OFFSET = 120.0f;

WebServer httpServer(80);
WiFiServer wsServer(81);
WiFiClient wsClient;
I2SClass i2s;

int32_t rawSamples[FRAME_SAMPLES];
int16_t pcmSamples[FRAME_SAMPLES];
uint32_t frameSequence = 0;
bool i2sReady = false;

struct __attribute__((packed)) AudioFrameHeader {
  char magic[4];
  uint16_t sampleCount;
  uint16_t flags;
  uint32_t sampleRate;
  uint32_t sequence;
  float rms;
  float dbfs;
  float dbspl;
};

String extractHeaderValue(const String &request, const String &name) {
  String lowerRequest = request;
  String lowerName = name;
  lowerRequest.toLowerCase();
  lowerName.toLowerCase();
  lowerName += ":";

  int start = lowerRequest.indexOf(lowerName);
  if (start < 0) return "";
  start += lowerName.length();

  int end = request.indexOf("\r\n", start);
  if (end < 0) end = request.length();

  String value = request.substring(start, end);
  value.trim();
  return value;
}

String makeWebSocketAccept(const String &key) {
  const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  String source = key + guid;

  unsigned char sha1[20];
  mbedtls_sha1(
    reinterpret_cast<const unsigned char *>(source.c_str()),
    source.length(),
    sha1
  );

  unsigned char encoded[32];
  size_t encodedLength = 0;
  mbedtls_base64_encode(encoded, sizeof(encoded), &encodedLength, sha1, sizeof(sha1));
  encoded[encodedLength] = '\0';
  return String(reinterpret_cast<char *>(encoded));
}

String readHttpRequest(WiFiClient &client) {
  String request;
  request.reserve(1400);

  uint32_t deadline = millis() + 1000;
  while (client.connected() && millis() < deadline) {
    while (client.available()) {
      char c = static_cast<char>(client.read());
      request += c;
      if (request.endsWith("\r\n\r\n") || request.length() > 1800) {
        return request;
      }
    }
    delay(1);
  }
  return request;
}

void drainWebSocketInput() {
  if (!wsClient || !wsClient.connected()) return;

  while (wsClient.available() >= 2) {
    uint8_t first = wsClient.read();
    uint8_t second = wsClient.read();
    uint8_t opcode = first & 0x0F;
    bool masked = (second & 0x80) != 0;
    uint64_t length = second & 0x7F;

    if (length == 126) {
      while (wsClient.available() < 2) delay(1);
      length = (static_cast<uint16_t>(wsClient.read()) << 8) | wsClient.read();
    } else if (length == 127) {
      length = 0;
      while (wsClient.available() < 8) delay(1);
      for (int i = 0; i < 8; i++) length = (length << 8) | wsClient.read();
    }

    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
      while (wsClient.available() < 4) delay(1);
      for (int i = 0; i < 4; i++) mask[i] = wsClient.read();
    }

    for (uint64_t i = 0; i < length; i++) {
      while (!wsClient.available() && wsClient.connected()) delay(1);
      if (!wsClient.connected()) return;
      uint8_t ignored = wsClient.read();
      if (masked) ignored ^= mask[i % 4];
      (void)ignored;
    }

    if (opcode == 0x8) {
      wsClient.stop();
      Serial.println("WebSocket client closed");
      return;
    }
  }
}

void handleWebSocket() {
  if (wsClient && wsClient.connected()) {
    drainWebSocketInput();
    return;
  }

  if (wsClient) wsClient.stop();

  WiFiClient candidate = wsServer.available();
  if (!candidate) return;

  String request = readHttpRequest(candidate);
  String key = extractHeaderValue(request, "Sec-WebSocket-Key");
  String upgrade = extractHeaderValue(request, "Upgrade");
  upgrade.toLowerCase();

  if (key.length() == 0 || upgrade != "websocket") {
    candidate.stop();
    return;
  }

  String accept = makeWebSocketAccept(key);
  candidate.print("HTTP/1.1 101 Switching Protocols\r\n");
  candidate.print("Upgrade: websocket\r\n");
  candidate.print("Connection: Upgrade\r\n");
  candidate.print("Sec-WebSocket-Accept: ");
  candidate.print(accept);
  candidate.print("\r\n\r\n");
  candidate.setNoDelay(true);

  wsClient = candidate;
  Serial.println("WebSocket client connected");
}

bool sendWebSocketBinary(const uint8_t *partA, size_t lenA, const uint8_t *partB, size_t lenB) {
  if (!wsClient || !wsClient.connected()) return false;

  size_t payloadLength = lenA + lenB;
  uint8_t header[10];
  size_t headerLength = 0;
  header[headerLength++] = 0x82;

  if (payloadLength <= 125) {
    header[headerLength++] = static_cast<uint8_t>(payloadLength);
  } else if (payloadLength <= 65535) {
    header[headerLength++] = 126;
    header[headerLength++] = static_cast<uint8_t>((payloadLength >> 8) & 0xFF);
    header[headerLength++] = static_cast<uint8_t>(payloadLength & 0xFF);
  } else {
    header[headerLength++] = 127;
    for (int shift = 56; shift >= 0; shift -= 8) {
      header[headerLength++] = static_cast<uint8_t>((payloadLength >> shift) & 0xFF);
    }
  }

  size_t written = 0;
  written += wsClient.write(header, headerLength);
  written += wsClient.write(partA, lenA);
  if (partB && lenB > 0) written += wsClient.write(partB, lenB);

  if (written != headerLength + payloadLength) {
    wsClient.stop();
    return false;
  }
  return true;
}

void readAndSendAudioFrame() {
  size_t bytesRead = i2s.readBytes(reinterpret_cast<char *>(rawSamples), sizeof(rawSamples));
  size_t samplesRead = bytesRead / sizeof(rawSamples[0]);
  if (samplesRead == 0) return;

  int64_t sum = 0;
  for (size_t i = 0; i < samplesRead; i++) sum += rawSamples[i] >> PCM_GAIN_SHIFT;
  int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(samplesRead));

  double squareSum = 0.0;
  for (size_t i = 0; i < samplesRead; i++) {
    int32_t sample = (rawSamples[i] >> PCM_GAIN_SHIFT) - mean;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    pcmSamples[i] = static_cast<int16_t>(sample);

    float normalized = static_cast<float>(pcmSamples[i]) / 32768.0f;
    squareSum += static_cast<double>(normalized) * normalized;
  }

  float rms = sqrt(squareSum / samplesRead);
  float dbfs = rms > 0.000001f ? 20.0f * log10f(rms) : -100.0f;
  float dbspl = dbfs + DB_SPL_OFFSET;

  AudioFrameHeader frame = {
    {'A', 'U', 'D', '1'},
    static_cast<uint16_t>(samplesRead),
    0,
    SAMPLE_RATE,
    frameSequence++,
    rms,
    dbfs,
    dbspl
  };

  sendWebSocketBinary(
    reinterpret_cast<const uint8_t *>(&frame),
    sizeof(frame),
    reinterpret_cast<const uint8_t *>(pcmSamples),
    samplesRead * sizeof(pcmSamples[0])
  );
}

void startWiFi() {
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");

    uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected: http://");
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.println("WiFi failed, falling back to AP mode");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, 6);
  Serial.print("AP mode: connect to ");
  Serial.print(AP_SSID);
  Serial.print(" then open http://");
  Serial.println(WiFi.softAPIP());
}

void startHttpServer() {
  httpServer.on("/", []() {
    httpServer.sendHeader("Cache-Control", "no-store");
    httpServer.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  httpServer.on("/health", []() {
    String json = "{";
    json += "\"sampleRate\":" + String(SAMPLE_RATE) + ",";
    json += "\"frameSamples\":" + String(FRAME_SAMPLES) + ",";
    json += "\"wsPort\":81,";
    json += "\"dbOffset\":" + String(DB_SPL_OFFSET, 1);
    json += "}";
    httpServer.send(200, "application/json", json);
  });

  httpServer.begin();
  wsServer.begin();
  Serial.println("HTTP server on port 80, WebSocket on port 81");
}

void startI2S() {
  i2s.setPins(I2S_BCLK_PIN, I2S_WS_PIN, -1, I2S_DIN_PIN);
  i2sReady = i2s.begin(
    I2S_MODE_STD,
    SAMPLE_RATE,
    I2S_DATA_BIT_WIDTH_32BIT,
    I2S_SLOT_MODE_MONO,
    MIC_I2S_SLOT
  );

  if (!i2sReady) {
    Serial.println("I2S init failed. Check pins, board target, and ESP32 core version.");
    return;
  }

  Serial.println("I2S microphone ready");
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("iot101 sound visualizer");

  startWiFi();
  startHttpServer();
  startI2S();
}

void loop() {
  httpServer.handleClient();
  handleWebSocket();

  if (i2sReady && wsClient && wsClient.connected()) {
    readAndSendAudioFrame();
  }

  delay(1);
}
