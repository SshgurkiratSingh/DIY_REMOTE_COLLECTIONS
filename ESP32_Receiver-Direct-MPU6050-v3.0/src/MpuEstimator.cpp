#include "MpuEstimator.h"

namespace
{
    const float RAD_TO_DEG_F = 57.2957795f;
    const float PI_F = 3.14159265f;

    float normalizeAngle(float angle)
    {
        while (angle > PI_F)
        {
            angle -= 2.0f * PI_F;
        }
        while (angle < -PI_F)
        {
            angle += 2.0f * PI_F;
        }
        return angle;
    }
}

bool MpuEstimator::begin(int sdaPin, int sclPin)
{
    Wire.begin(sdaPin, sclPin);

    if (!mpu_.begin())
    {
        return false;
    }

    mpu_.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);

    yawRad_ = 0.0f;
    return true;
}

bool MpuEstimator::update(float dt, ImuState &stateOut)
{
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    mpu_.getEvent(&accel, &gyro, &temp);

    yawRad_ += gyro.gyro.z * dt;
    yawRad_ = normalizeAngle(yawRad_);

    const float ax = accel.acceleration.x;
    const float ay = accel.acceleration.y;
    const float az = accel.acceleration.z;

    stateOut.yawRad = yawRad_;
    stateOut.pitchDeg = atan2f(-ax, sqrtf((ay * ay) + (az * az))) * RAD_TO_DEG_F;
    stateOut.rollDeg = atan2f(ay, az) * RAD_TO_DEG_F;
    stateOut.forwardAccel = ay;

    return true;
}
