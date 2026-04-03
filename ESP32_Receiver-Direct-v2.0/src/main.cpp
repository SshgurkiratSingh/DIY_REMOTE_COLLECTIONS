#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct struct_message
{
  uint16_t joyX;
  uint16_t joyY;
  uint16_t potValue;
  bool toggle1;
  bool toggle2;
  bool push1;
  bool push2;
} struct_message;

typedef struct struct_reply
{
  uint8_t sensorValue;
  uint8_t counter;
  uint8_t flags;
} struct_reply;

struct MotorDriver
{
  int rpwmPin;
  int lpwmPin;
  int lenPin;
  int renPin;
  int chanForward;
  int chanReverse;
};

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

const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_MAX = 255;
const int JOY_MAX = 4095;
const int JOY_DEADZONE = 120;
const int BURST_SPEED_LIMIT = 255;

// Ramp control
const float MIN_RAMP_RATE = 100.0;  // 2.5 seconds to full speed at min pot
const float MAX_RAMP_RATE = 1000.0; // 0.25 seconds to full speed at max pot
float currentRampRate = MAX_RAMP_RATE;

uint8_t replyCounter = 0;

// Motor state globals
int targetLeftSpeed = 0;
int targetRightSpeed = 0;
float currentLeftSpeed = 0.0;
float currentRightSpeed = 0.0;
bool slowRampEnabled = false;
bool burstActive = false;
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

void initMotor(const MotorDriver &motor)
{
  ledcSetup(motor.chanForward, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(motor.chanReverse, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(motor.rpwmPin, motor.chanForward);
  ledcAttachPin(motor.lpwmPin, motor.chanReverse);

  pinMode(motor.lenPin, OUTPUT);
  pinMode(motor.renPin, OUTPUT);
  digitalWrite(motor.lenPin, HIGH);
  digitalWrite(motor.renPin, HIGH);

  ledcWrite(motor.chanForward, 0);
  ledcWrite(motor.chanReverse, 0);
}

void setMotor(const MotorDriver &motor, int speed)
{
  speed = constrain(speed, -PWM_MAX, PWM_MAX);

  if (speed >= 0)
  {
    ledcWrite(motor.chanForward, speed);
    ledcWrite(motor.chanReverse, 0);
  }
  else
  {
    ledcWrite(motor.chanForward, 0);
    ledcWrite(motor.chanReverse, -speed);
  }
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

uint8_t readBuiltInSensorValue()
{
#if CONFIG_IDF_TARGET_ESP32
  return static_cast<uint8_t>(hallRead() & 0xFF);
#else
  return static_cast<uint8_t>(millis() & 0xFF);
#endif
}

uint8_t buildFlagsByte(const struct_message &incomingData)
{
  uint8_t flags = 0;
  flags |= incomingData.toggle1 ? (1U << 0) : 0;
  flags |= incomingData.toggle2 ? (1U << 1) : 0;
  flags |= incomingData.push1 ? (1U << 2) : 0;
  flags |= incomingData.push2 ? (1U << 3) : 0;
  return flags;
}

bool ensurePeer(const uint8_t *macAddress)
{
  if (esp_now_is_peer_exist(macAddress))
  {
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macAddress, ESP_NOW_ETH_ALEN);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo) == ESP_OK;
}

void sendReply(const uint8_t *macAddress, const struct_message &incomingData)
{
  if (!ensurePeer(macAddress))
  {
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

  if (sendResult == ESP_OK)
  {
    Serial.println("Reply sent successfully.");
  }
  else
  {
    Serial.print("Reply send failed: ");
    Serial.println(sendResult);
  }
}

int mapJoystickSigned(uint16_t raw)
{
  int mapped = map(raw, 0, JOY_MAX, -PWM_MAX, PWM_MAX);
  if (abs(mapped) < JOY_DEADZONE)
  {
    return 0;
  }
  return mapped;
}

int speedLimitFromPot(uint16_t potValue)
{
  return map(potValue, 0, JOY_MAX, 0, PWM_MAX);
}

void applyDriveFromMessage(const struct_message &incomingData)
{
  burstActive = incomingData.push1;
  slowRampEnabled = incomingData.toggle1;

  // Let the potentiometer dictate the speed limit as well as the dynamic ramp rate
  const int speedLimit = burstActive ? BURST_SPEED_LIMIT : speedLimitFromPot(incomingData.potValue);

  // High pot value = high speed limit = fast ramp rate
  // Low pot value = low speed limit = slow ramp rate
  currentRampRate = map((long)incomingData.potValue, 0, JOY_MAX, (long)MIN_RAMP_RATE, (long)MAX_RAMP_RATE);

  const int throttle = mapJoystickSigned(incomingData.joyY);
  const int steer = mapJoystickSigned(incomingData.joyX);

  int leftSpeed = throttle + steer;
  int rightSpeed = throttle - steer;

  targetLeftSpeed = constrain(leftSpeed, -speedLimit, speedLimit);
  targetRightSpeed = constrain(rightSpeed, -speedLimit, speedLimit);

  Serial.print("Slow ramp: ");
  Serial.println(slowRampEnabled ? "ON" : "OFF");
  Serial.print("Burst mode: ");
  Serial.println(burstActive ? "ON" : "OFF");
  Serial.print("Speed limit: ");
  Serial.println(speedLimit);
  Serial.print("Target Left: ");
  Serial.println(targetLeftSpeed);
  Serial.print("Target Right: ");
  Serial.println(targetRightSpeed);
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
  printReceivedData(incomingData);
  applyDriveFromMessage(incomingData);
  // sendReply(macAddress, incomingData);
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

  initMotor(LEFT_MOTOR);
  initMotor(RIGHT_MOTOR);

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
  float dt = (now - lastTick) / 1000.0f;
  lastTick = now;

  if (dt > 0.5f)
    dt = 0.5f; // Clamp dt to prevent massive jumps on lag

  // If we are commanding a stop (joysticks near center), stop instantly regardless of ramp setting
  if (targetLeftSpeed == 0 && targetRightSpeed == 0)
  {
    currentLeftSpeed = 0;
    currentRightSpeed = 0;
  }
  else if (slowRampEnabled && !burstActive)
  {
    float step = currentRampRate * dt;

    if (currentLeftSpeed < targetLeftSpeed)
    {
      currentLeftSpeed += step;
      if (currentLeftSpeed > targetLeftSpeed)
        currentLeftSpeed = targetLeftSpeed;
    }
    else if (currentLeftSpeed > targetLeftSpeed)
    {
      currentLeftSpeed -= step;
      if (currentLeftSpeed < targetLeftSpeed)
        currentLeftSpeed = targetLeftSpeed;
    }

    if (currentRightSpeed < targetRightSpeed)
    {
      currentRightSpeed += step;
      if (currentRightSpeed > targetRightSpeed)
        currentRightSpeed = targetRightSpeed;
    }
    else if (currentRightSpeed > targetRightSpeed)
    {
      currentRightSpeed -= step;
      if (currentRightSpeed < targetRightSpeed)
        currentRightSpeed = targetRightSpeed;
    }
  }
  else
  {
    // Instant response if ramp disabled or bursting
    currentLeftSpeed = targetLeftSpeed;
    currentRightSpeed = targetRightSpeed;
  }

  setMotor(LEFT_MOTOR, (int)currentLeftSpeed);
  setMotor(RIGHT_MOTOR, (int)currentRightSpeed);

  delay(20);
}