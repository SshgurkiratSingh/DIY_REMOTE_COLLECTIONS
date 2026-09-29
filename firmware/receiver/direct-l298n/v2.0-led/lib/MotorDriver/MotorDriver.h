#pragma once

#include <Arduino.h>
#include "Config.h"

class MotorDriver {
public:
    MotorDriver();
    void begin();
    
    // Process raw joystick values, apply deadzone, and calculate target speeds
    void mixDrive(uint16_t throttleRaw, uint16_t steerRaw);
    
    // Emergency stop (bypasses ramping)
    void stop();
    
    // Call frequently to ramp current speeds towards target speeds
    void update();
    
    // Getters for sync logic
    int getCurrentSpeedA() const { return currentSpeedA; }
    int getCurrentSpeedB() const { return currentSpeedB; }
    int getTargetFwd() const { return targetFwd; }
    int getTargetTurn() const { return targetTurn; }

private:
    void setMotorA(int speed);
    void setMotorB(int speed);
    int rampTowards(int current, int target, int step);

    int targetSpeedA;
    int currentSpeedA;
    int targetSpeedB;
    int currentSpeedB;
    
    // Stored components for external logic (like LED sync)
    int targetFwd;
    int targetTurn;

    unsigned long lastUpdateMs;
};
