#pragma once

#include <Arduino.h>
#include "ControlTypes.h"

class MotorController
{
public:
    MotorController(const MotorDriver &leftMotor, const MotorDriver &rightMotor);

    void begin();
    void setTargets(int left, int right, bool enableRamp, bool burstActive, float rampRate);
    void update(float dt);

private:
    void initMotor(const MotorDriver &motor);
    void setMotor(const MotorDriver &motor, int speed);

    MotorDriver left_;
    MotorDriver right_;

    int targetLeft_;
    int targetRight_;
    float currentLeft_;
    float currentRight_;
    bool enableRamp_;
    bool burstActive_;
    float rampRate_;
};
