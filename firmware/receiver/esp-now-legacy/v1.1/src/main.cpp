#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct struct_message {
  uint16_t joyX;
  uint16_t joyY;
  uint16_t potValue;
  int32_t encoderPos;
  bool encSw;
  bool toggle1;
  bool toggle2;
  bool push1;
  bool push2;
} struct_message;

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

  Serial.print("encoderPos: ");
  Serial.println(incomingData.encoderPos);

  Serial.print("encSw: ");
  Serial.println(incomingData.encSw);

  Serial.print("toggle1: ");
  Serial.println(incomingData.toggle1);

  Serial.print("toggle2: ");
  Serial.println(incomingData.toggle2);

  Serial.print("push1: ");
  Serial.println(incomingData.push1);

  Serial.print("push2: ");
  Serial.println(incomingData.push2);
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int dataLen) {
  if (recvInfo == nullptr || recvInfo->src_addr == nullptr) {
    Serial.println("Received data, but MAC address was unavailable.");
    return;
  }

  Serial.print("Sender MAC: ");
  Serial.println(formatMacAddress(recvInfo->src_addr));
#else
void onDataReceive(const uint8_t *macAddress, const uint8_t *data, int dataLen) {
  Serial.print("Sender MAC: ");
  Serial.println(formatMacAddress(macAddress));
#endif

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
  Serial.println();
}

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
