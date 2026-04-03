#include <Arduino.h>

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

void stopMotor(const MotorDriver &motor)
{
  ledcWrite(motor.chanForward, 0);
  ledcWrite(motor.chanReverse, 0);
}

void setup()
{
  Serial.begin(115200);

  initMotor(LEFT_MOTOR);
  initMotor(RIGHT_MOTOR);

  Serial.println("ESP32 TWO-MOTOR CONTROLLER READY");
}

void loop()
{
  Serial.println("TEST: LEFT FORWARD / RIGHT FORWARD");
  setMotor(LEFT_MOTOR, 180);
  setMotor(RIGHT_MOTOR, 180);
  delay(2000);

  Serial.println("TEST: LEFT REVERSE / RIGHT REVERSE");
  setMotor(LEFT_MOTOR, -180);
  setMotor(RIGHT_MOTOR, -180);
  delay(2000);

  Serial.println("TEST: TURN LEFT (LEFT STOP, RIGHT FORWARD)");
  setMotor(LEFT_MOTOR, 0);
  setMotor(RIGHT_MOTOR, 200);
  delay(1500);

  Serial.println("TEST: TURN RIGHT (LEFT FORWARD, RIGHT STOP)");
  setMotor(LEFT_MOTOR, 200);
  setMotor(RIGHT_MOTOR, 0);
  delay(1500);

  Serial.println("TEST: STOP BOTH");
  stopMotor(LEFT_MOTOR);
  stopMotor(RIGHT_MOTOR);
  delay(2000);
}