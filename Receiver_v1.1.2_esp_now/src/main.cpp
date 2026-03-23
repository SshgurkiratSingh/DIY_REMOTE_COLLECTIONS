#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct struct_message {
  uint16_t joyX;
  uint16_t joyY;
  uint16_t potValue;
  bool toggle1;
  bool toggle2;
  bool push1;
  bool push2;
} struct_message;

typedef struct struct_reply {
  uint8_t sensorValue;
  uint8_t counter;
  uint8_t flags;
} struct_reply;

uint8_t replyCounter = 0;

String formatMacAddress(const uint8_t *macAddress) {
  char macStr[18];
  snprintf(
      macStr,
      sizeof(macStr),
      "%02X:%02X:%02X:%02X:%02X:%02X",
      macAddress[0],
      macAddress[1],
      macAddress[2],
      macAddress[3],
      macAddress[4],
      macAddress[5]);
  return String(macStr);
}

void printReceivedData(const struct_message &incomingData) {
  Serial.print("joyX: ");
  Serial.println(incomingData.joyX);

  Serial.print("joyY: ");
  Serial.println(incomingData.joyY);

  Serial.print("potValue: ");
  Serial.println(incomingData.potValue);

  Serial.print("toggle1: ");
  Serial.println(incomingData.toggle1);

  Serial.print("toggle2: ");
  Serial.println(incomingData.toggle2);

  Serial.print("push1: ");
  Serial.println(incomingData.push1);

  Serial.print("push2: ");
  Serial.println(incomingData.push2);
}

uint8_t readBuiltInSensorValue() {
#if CONFIG_IDF_TARGET_ESP32
  return static_cast<uint8_t>(hallRead() & 0xFF);
#else
  return static_cast<uint8_t>(millis() & 0xFF);
#endif
}

uint8_t buildFlagsByte(const struct_message &incomingData) {
  uint8_t flags = 0;
  flags |= incomingData.toggle1 ? (1U << 0) : 0;
  flags |= incomingData.toggle2 ? (1U << 1) : 0;
  flags |= incomingData.push1 ? (1U << 2) : 0;
  flags |= incomingData.push2 ? (1U << 3) : 0;
  return flags;
}

bool ensurePeer(const uint8_t *macAddress) {
  if (esp_now_is_peer_exist(macAddress)) {
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macAddress, ESP_NOW_ETH_ALEN);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

void sendReply(const uint8_t *macAddress, const struct_message &incomingData) {
  if (!ensurePeer(macAddress)) {
    Serial.println("Failed to add sender as ESP-NOW peer.");
    return;
  }

  struct_reply replyData = {
      readBuiltInSensorValue(),
      replyCounter++,
      buildFlagsByte(incomingData),
  };

  esp_err_t sendResult =
      esp_now_send(macAddress, reinterpret_cast<const uint8_t *>(&replyData), sizeof(replyData));

  Serial.print("Reply sensorValue: ");
  Serial.println(replyData.sensorValue);
  Serial.print("Reply counter: ");
  Serial.println(replyData.counter);
  Serial.print("Reply flags: ");
  Serial.println(replyData.flags);

  if (sendResult == ESP_OK) {
    Serial.println("Reply sent successfully.");
  } else {
    Serial.print("Reply send failed: ");
    Serial.println(sendResult);
  }
}

void handleReceivedPacket(const uint8_t *macAddress, const uint8_t *data, int dataLen) {
  Serial.print("Sender MAC: ");
  Serial.println(formatMacAddress(macAddress));

  Serial.print("Bytes received: ");
  Serial.println(dataLen);

  if (dataLen != sizeof(struct_message)) {
    Serial.println("Packet size mismatch. Expected struct_message payload.");
    Serial.println();
    return;
  }

  struct_message incomingData;
  memcpy(&incomingData, data, sizeof(incomingData));
  printReceivedData(incomingData);
  sendReply(macAddress, incomingData);
  Serial.println();
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int dataLen) {
  if (recvInfo == nullptr || recvInfo->src_addr == nullptr) {
    Serial.println("Received data, but MAC address was unavailable.");
    return;
  }

  handleReceivedPacket(recvInfo->src_addr, data, dataLen);
}
#else
void onDataReceive(const uint8_t *macAddress, const uint8_t *data, int dataLen) {
  handleReceivedPacket(macAddress, data, dataLen);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("ESP-NOW Receiver Starting...");
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed.");
    return;
  }

  esp_now_register_recv_cb(onDataReceive);
  Serial.println("ESP-NOW receiver ready.");
}

void loop() {
  delay(100);
}
