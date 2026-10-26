#pragma once

#include <Arduino.h>
#include "config.h"

// Initialize motor driver pins and LEDC PWM channels
void initMotors();

// Set motor speeds (-255 to +255).
// Positive: forward, Negative: reverse, 0: stop
void setMotorSpeed(int16_t leftSpeed, int16_t rightSpeed);

// Full stop on both motors immediately
void emergencyStop();

// Put TB6612 motor driver into standby mode (true = standby/Hi-Z, false = active)
void setMotorStandby(bool standby);

// Motor testing routine for verification of wiring & direction
// action: "lf", "lr", "rf", "rr", "forward", "reverse", "stop"
void testMotor(const String& action, int16_t speed);

// Safety watchdog: called whenever sensors are read
void notifyMotorSafetyFeed();

// Checks if safety timeout has elapsed; stops motors if timed out
bool checkMotorSafety();
