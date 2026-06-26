#include <Arduino.h>
#include "MotorController.h"
#include "Receiver.h"
#include "IMUHandler.h"
#include "DriveLogic.h"

// Define BTS7960 Pins
// Left Motor
const int L_RPWM = 5;
const int L_LPWM = 6;
const int L_LEN  = 7;
const int L_REN  = 8;

// Right Motor  
const int R_RPWM = 9;
const int R_LPWM = 10;
const int R_LEN  = 11;
const int R_REN  = 12;

// Create Component Instances
MotorController motors(L_RPWM, L_LPWM, L_LEN, L_REN,
                       R_RPWM, R_LPWM, R_LEN, R_REN);

Receiver rcReceiver;
IMUHandler imuHandler;
DriveLogic* driveLogic;

void setup() {
  // Motor Setup
  motors.begin();

  // IMU Setup
  imuHandler.begin();

  // Receiver Setup (also handles Serial.begin)
  rcReceiver.begin();
  
  // Drive Logic Setup
  driveLogic = new DriveLogic(&motors, &imuHandler);

  Serial.println("System Initialized.");
}

void loop() {
  // Update sensors and inputs
  rcReceiver.update();
  imuHandler.update();

  // Fetch updated data
  ReceiverData rcData = rcReceiver.getData();

  // Execute drive modes
  driveLogic->update(rcData);

  // Optional: Debugging Prints for tuning
  // Serial.print("Yaw: "); Serial.print(imuHandler.getYaw());
  // Serial.print(" | Mode: "); Serial.print(rcData.mode);
  // Serial.print(" | ch1: "); Serial.print(rcData.ch1);
  // Serial.print(" | ch2: "); Serial.println(rcData.ch2);

  delay(20);
}
