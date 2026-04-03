#pragma once

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "ControlTypes.h"

class MpuEstimator
{
public:
    bool begin(int sdaPin, int sclPin);
    bool update(float dt, ImuState &stateOut);

private:
    Adafruit_MPU6050 mpu_;
    float yawRad_ = 0.0f;
};
