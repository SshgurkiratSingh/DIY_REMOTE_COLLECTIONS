#include <Arduino.h>
#include "MotorController.h"
#include "Receiver.h"
#include "DriveLogic.h"

// Define L298N Pins
// Left Motor
const int L_EN = 5;
const int L_IN1 = 8;
const int L_IN2 = 7;

// Right Motor
const int R_EN = 6;
const int R_IN1 = 12;
const int R_IN2 = 13;

// Create Component Instances
MotorController motors(L_EN, L_IN1, L_IN2,
                       R_EN, R_IN1, R_IN2);

Receiver rcReceiver;
DriveLogic *driveLogic;

void setup()
{
  // Motor Setup
  motors.begin();

  // Receiver Setup (also handles Serial.begin)
  rcReceiver.begin();

  // Drive Logic Setup
  driveLogic = new DriveLogic(&motors);

  Serial.println("System Initialized.");
}

void loop()
{
  // Update sensors and inputs
  rcReceiver.update();

  // Fetch updated data
  ReceiverData rcData = rcReceiver.getData();

  // Execute drive modes
  driveLogic->update(rcData);

  // Debugging Prints for tuning
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 100)
  { // Limit serial spam to ~10Hz
    Serial.print("Mode: ");
    Serial.print(rcData.mode);
    Serial.print(" | ch1: ");
    Serial.print(rcData.ch1);
    Serial.print(" | ch2: ");
    Serial.println(rcData.ch2);
    lastPrintTime = millis();
  }

  delay(20);
}
