#pragma once

#include <Arduino.h>
#include "ControlTypes.h"

class DriveModes
{
public:
    DriveModes();

    void updateHomeDirectionOnPush2Rising(bool push2Pressed, float currentYawRad);
    DriveOutput compute(const struct_message &msg, const ImuState &imu, float dt);

private:
    int mapJoystickSigned(uint16_t raw) const;
    int speedLimitFromPot(uint16_t potValue) const;
    float normalizeAngle(float angleRad) const;

    bool prevPush2_;
    float homeYawRad_;
    float headingHoldYawRad_;
    bool headingHoldArmed_;
    float tractionScale_;
};
