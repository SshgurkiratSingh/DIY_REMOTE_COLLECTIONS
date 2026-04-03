#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "ControlTypes.h"
#include "MotorController.h"
#include "MpuEstimator.h"
#include "DriveModes.h"

// Adjust these pin mappings to match your physical wiring.
const MotorDriver LEFT_MOTOR = {
    .rpwmPin = 18,
    .lpwmPin = 19,
    .lenPin = 21,
    .renPin = 22,
    .chanForward = 0,
    .chanReverse = 1,
};

const MotorDriver RIGHT_MOTOR = {
    .rpwmPin = 25,
    .lpwmPin = 26,
    .lenPin = 27,
    .renPin = 14,
    .chanForward = 2,
    .chanReverse = 3,
};

const int I2C_SDA_PIN = 32;
const int I2C_SCL_PIN = 33;
const uint32_t COMMAND_TIMEOUT_MS = 500;

MotorController motorController(LEFT_MOTOR, RIGHT_MOTOR);
MpuEstimator imuEstimator;
DriveModes driveModes;

portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
struct_message latestCommand = {2048, 2048, 4095, false, false, false, false};
bool hasFreshCommand = false;
uint32_t lastCommandMs = 0;

ImuState imuState = {0.0f, 0.0f, 0.0f, 0.0f};
unsigned long lastTick = 0;

String formatMacAddress(const uint8_t *macAddress)
{
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

void printReceivedData(const struct_message &incomingData)
{
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

  Serial.print("push1 (burst): ");
  Serial.println(incomingData.push1);

  Serial.print("push2: ");
  Serial.println(incomingData.push2);
}

struct_message neutralCommand()
{
  return {2048, 2048, 0, false, false, false, false};
}

void handleReceivedPacket(const uint8_t *macAddress, const uint8_t *data, int dataLen)
{
  Serial.print("Sender MAC: ");
  Serial.println(formatMacAddress(macAddress));

  Serial.print("Bytes received: ");
  Serial.println(dataLen);

  if (dataLen != sizeof(struct_message))
  {
    Serial.println("Packet size mismatch. Expected struct_message payload.");
    Serial.println();
    return;
  }

  struct_message incomingData;
  memcpy(&incomingData, data, sizeof(incomingData));

  portENTER_CRITICAL(&commandMux);
  latestCommand = incomingData;
  hasFreshCommand = true;
  lastCommandMs = millis();
  portEXIT_CRITICAL(&commandMux);

  printReceivedData(incomingData);
  Serial.println();
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int dataLen)
{
  if (recvInfo == nullptr || recvInfo->src_addr == nullptr)
  {
    Serial.println("Received data, but MAC address was unavailable.");
    return;
  }

  handleReceivedPacket(recvInfo->src_addr, data, dataLen);
}
#else
void onDataReceive(const uint8_t *macAddress, const uint8_t *data, int dataLen)
{
  handleReceivedPacket(macAddress, data, dataLen);
}
#endif

void setup()
{
  Serial.begin(115200);
  delay(1000);

  motorController.begin();

  if (!imuEstimator.begin(I2C_SDA_PIN, I2C_SCL_PIN))
  {
    Serial.println("MPU6050 init failed. Check wiring/power.");
  }
  else
  {
    Serial.println("MPU6050 init OK.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("ESP-NOW Receiver Starting...");
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init failed.");
    return;
  }

  esp_now_register_recv_cb(onDataReceive);
  Serial.println("ESP-NOW receiver ready.");
  lastTick = millis();
}

void loop()
{
  unsigned long now = millis();
  float dt = lastTick == 0 ? 0.02f : (now - lastTick) / 1000.0f;
  lastTick = now;

  if (dt > 0.5f)
  {
    dt = 0.5f;
  }

  imuEstimator.update(dt, imuState);

  struct_message commandSnapshot;
  bool freshSnapshot;
  uint32_t lastCommandSnapshotMs;

  portENTER_CRITICAL(&commandMux);
  commandSnapshot = latestCommand;
  freshSnapshot = hasFreshCommand;
  lastCommandSnapshotMs = lastCommandMs;
  portEXIT_CRITICAL(&commandMux);

  if (!freshSnapshot || (now - lastCommandSnapshotMs > COMMAND_TIMEOUT_MS))
  {
    commandSnapshot = neutralCommand();
  }

  driveModes.updateHomeDirectionOnPush2Rising(commandSnapshot.push2, imuState.yawRad);
  DriveOutput out = driveModes.compute(commandSnapshot, imuState, dt);

  motorController.setTargets(
      out.targetLeft,
      out.targetRight,
      out.enableRamp,
      out.burstActive,
      out.rampRate);
  motorController.update(dt);

  delay(20);
}